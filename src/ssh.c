#include "ssh.h"

int verify_knownhost(ssh_session session) {
  enum ssh_known_hosts_e state;
  unsigned char *hash = NULL;
  ssh_key srv_pubkey = NULL;
  size_t hlen;
  char buf[10];
  char *hexa;
  char *p;
  int cmp;
  int rc;

  rc = ssh_get_server_publickey(session, &srv_pubkey);
  if (rc < 0) {
    return -1;
  }

  rc =
      ssh_get_publickey_hash(srv_pubkey, SSH_PUBLICKEY_HASH_SHA1, &hash, &hlen);
  ssh_key_free(srv_pubkey);
  if (rc < 0) {
    return -1;
  }

  state = ssh_session_is_known_server(session);
  switch (state) {
    case SSH_KNOWN_HOSTS_OK:
      /* OK */
      break;
    case SSH_KNOWN_HOSTS_CHANGED:
      fprintf(stderr,
              "Host key for server changed. it is now: %s\n"
              "For security reasons, connection will be stopped\n",
              hash);
      ssh_clean_pubkey_hash(&hash);

      return -1;
    case SSH_KNOWN_HOSTS_OTHER:
      fprintf(stderr,
              "The host key for this server was not found but an other"
              "type of key exists.\n");
      fprintf(stderr,
              "An attacker might change the default server key to"
              "confuse your client into thinking the key does not exist\n");
      ssh_clean_pubkey_hash(&hash);

      return -1;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
      fprintf(stderr, "Could not find known host file.\n");
      fprintf(stderr,
              "If you accept the host key here, the file will be"
              "automatically created.\n");

      /* FALL THROUGH to SSH_SERVER_NOT_KNOWN behavior */

    case SSH_KNOWN_HOSTS_UNKNOWN:
      hexa = ssh_get_hexa(hash, hlen);
      fprintf(stderr, "The server is unknown. Do you trust the host key?\n");
      fprintf(stderr, "Public key hash: %s\n", hexa);
      ssh_string_free_char(hexa);
      ssh_clean_pubkey_hash(&hash);
      p = fgets(buf, sizeof(buf), stdin);
      if (p == NULL) {
        return -1;
      }

      cmp = strncasecmp(buf, "yes", 3);
      if (cmp != 0) {
        return -1;
      }

      rc = ssh_session_update_known_hosts(session);
      if (rc < 0) {
        fprintf(stderr, "Error %s\n", strerror(errno));
        return -1;
      }
      break;

    case SSH_KNOWN_HOSTS_ERROR:
      fprintf(stderr, "Error %s", ssh_get_error(session));
      ssh_clean_pubkey_hash(&hash);
      return -1;
  }

  ssh_clean_pubkey_hash(&hash);
  return 0;
}

int authenticate_privkey(ssh_session session, ssh_key privkey) {
  int rc = ssh_userauth_publickey(session, "root", privkey);
  if (rc == SSH_AUTH_ERROR) {
    fprintf(stderr, "Authentication failed: %s\n", ssh_get_error(session));
    return SSH_AUTH_ERROR;
  }
  return rc;
}

int authenticate_with_password(ssh_session session) {
  const char buf_size = 16;
  char password[buf_size] = {};
  int rc = ssh_getpass("Input ssh password: ", password, buf_size, 0, 0);
  if (rc == -1) {
    fprintf(stderr, "Failed to read password!\n");
    return rc;
  }

  rc = ssh_userauth_password(session, "root", password);
  if (rc != SSH_AUTH_SUCCESS) {
    fprintf(stderr, "Error authenticating with password: %s\n",
            ssh_get_error(session));
  }
  return rc;
}

int print_command_output(ssh_session session, const char *cmd) {
  ssh_channel channel;
  const int buffer_size = 256;
  char buffer[buffer_size];
  int nbytes;

  channel = ssh_channel_new(session);
  if (channel == NULL) {
    fprintf(stderr, "Failed to create channel: %s\n", ssh_get_error(session));
    return SSH_ERROR;
  }

  int rc = ssh_channel_open_session(channel);
  if (rc != SSH_OK) {
    ssh_channel_free(channel);
    return rc;
  }

  rc = ssh_channel_request_exec(channel, cmd);
  if (rc != SSH_OK) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return rc;
  }

  nbytes = ssh_channel_read(channel, buffer, buffer_size, 0);
  while (nbytes > 0) {
    if (write(1, buffer, nbytes) != (unsigned int)nbytes) {
      ssh_channel_close(channel);
      ssh_channel_free(channel);
    }
    nbytes = ssh_channel_read(channel, buffer, buffer_size, 0);
  }

  if (nbytes < 0) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return SSH_ERROR;
  }

  /* Cleanup */
  ssh_channel_send_eof(channel);
  ssh_channel_close(channel);
  ssh_channel_free(channel);
}

int create_ssh_session(ssh_session *session, const char *address,
                       const int port, const char *privkey) {
  *session = ssh_new();
  if (*session == NULL) {
    fprintf(stderr, "Couldn't create SSH session.\n");
    return (SSH_ERROR);
  }

  /* SSH Connection Config */
  ssh_options_set(*session, SSH_OPTIONS_HOST, address);
  ssh_options_set(*session, SSH_OPTIONS_PORT, &port);
  ssh_options_set(*session, SSH_OPTIONS_HOSTKEYS, "+ssh-rsa");
  ssh_options_set(*session, SSH_OPTIONS_PUBLICKEY_ACCEPTED_TYPES, "+ssh-rsa");

  /* Try to establish connection */
  if (ssh_connect(*session) != SSH_OK) {
    fprintf(stderr, "Error connecting to host: %s\n", ssh_get_error(*session));
    ssh_free(*session);
    return (SSH_ERROR);
  }

  /* Verify server's identity */
  if (verify_knownhost(*session) < 0) {
    ssh_disconnect(*session);
    ssh_free(*session);
    return (SSH_ERROR);
  }

  /* Authenticate ourselves */
  if (strlen(privkey) != 0) {
    ssh_key key = NULL;
    const char buf_size = 64;
    char password[buf_size] = {};
    int rc;
    // try to use empty password first
    if(ssh_pki_import_privkey_file(privkey, NULL, NULL, NULL, &key)) {
      rc = ssh_getpass("Input private key password: ", password, buf_size, 0, 0);
      if (rc == -1) {
        fprintf(stderr, "Failed to read password!\n");
        return rc;
      }
      rc = ssh_pki_import_privkey_file(privkey, strlen(password) != 0 ? password : NULL, NULL, NULL, &key);
      switch (rc) {
        case SSH_EOF:
          fprintf(stderr, "Error reading private key! Either the file doesn't exist or permission denied.\n");
          ssh_disconnect(*session);
          ssh_free(*session);
          return (SSH_ERROR);
          break;
        case SSH_ERROR:
          fprintf(stderr, "Error reading the private key.\n");
          ssh_disconnect(*session);
          ssh_free(*session);
          return (SSH_ERROR);
          break;
        default:
          break;
      }
    }

    rc = authenticate_privkey(*session, key);
    if (key)
      ssh_key_free(key);
    if (rc != SSH_AUTH_SUCCESS) {
      fprintf(stderr, "SSH publickey authentication failed.\n");
      ssh_disconnect(*session);
      ssh_free(*session);
      return (SSH_ERROR);
    }
  } else {
    int rc = authenticate_with_password(*session);
    if (rc != SSH_AUTH_SUCCESS) {
      fprintf(stderr, "SSH password authentication failed.\n");
      ssh_disconnect(*session);
      ssh_free(*session);
      return (SSH_ERROR);
    }
  }
  return 0;
}