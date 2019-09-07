/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transport Layer Security
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>

#include <winpr/stream.h>
#include <freerdp/utils/tcp.h>
#include <freerdp/utils/ringbuffer.h>

#include <freerdp/crypto/tls.h>
#include "../core/tcp.h"

static CryptoCert tls_get_certificate(rdpTls* tls, BOOL peer)
{
	CryptoCert cert;
	X509* remote_cert;

	if (peer)
		remote_cert = SSL_get_peer_certificate(tls->ssl);
	else
		remote_cert = SSL_get_certificate(tls->ssl);

	if (!remote_cert)
	{
		fprintf(stderr, "%s: failed to get the server TLS certificate\n", __FUNCTION__);
		return NULL;
	}

	cert = malloc(sizeof(*cert));
	if (!cert)
	{
		X509_free(remote_cert);
		return NULL;
	}

	cert->px509 = remote_cert;
	return cert;
}

static void tls_free_certificate(CryptoCert cert)
{
	X509_free(cert->px509);
	free(cert);
}

#define TLS_SERVER_END_POINT	"tls-server-end-point:"

SecPkgContext_Bindings* tls_get_channel_bindings(X509* cert)
{
	int PrefixLength;
	BYTE CertificateHash[32];
	UINT32 CertificateHashLength;
	BYTE* ChannelBindingToken;
	UINT32 ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;
	SecPkgContext_Bindings* ContextBindings;

	ZeroMemory(CertificateHash, sizeof(CertificateHash));
	X509_digest(cert, EVP_sha256(), CertificateHash, &CertificateHashLength);

	PrefixLength = strlen(TLS_SERVER_END_POINT);
	ChannelBindingTokenLength = PrefixLength + CertificateHashLength;

	ContextBindings = (SecPkgContext_Bindings*) calloc(1, sizeof(SecPkgContext_Bindings));
	if (!ContextBindings)
		return NULL;

	ContextBindings->BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) + ChannelBindingTokenLength;
	ChannelBindings = (SEC_CHANNEL_BINDINGS*) calloc(1, ContextBindings->BindingsLength);
	if (!ChannelBindings)
		goto out_free;
	ContextBindings->Bindings = ChannelBindings;

	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*) ChannelBindings)[ChannelBindings->dwApplicationDataOffset];

	strcpy((char*) ChannelBindingToken, TLS_SERVER_END_POINT);
	CopyMemory(&ChannelBindingToken[PrefixLength], CertificateHash, CertificateHashLength);

	return ContextBindings;

out_free:
	free(ContextBindings);
	return NULL;
}


BOOL tls_prepare(rdpTls* tls, BIO *underlying, const SSL_METHOD *method, int options, BOOL clientMode)
{
	tls->ctx = SSL_CTX_new(method);
	if (!tls->ctx)
	{
		fprintf(stderr, "%s: SSL_CTX_new failed\n", __FUNCTION__);
		return FALSE;
	}

	SSL_CTX_set_mode(tls->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);

	SSL_CTX_set_options(tls->ctx, options);
	SSL_CTX_set_read_ahead(tls->ctx, 1);

	tls->bio = BIO_new_ssl(tls->ctx, clientMode);
	if (BIO_get_ssl(tls->bio, &tls->ssl) < 0)
	{
		fprintf(stderr, "%s: unable to retrieve the SSL of the connection\n", __FUNCTION__);
		return FALSE;
	}

	BIO_push(tls->bio, underlying);
	return TRUE;
}

int tls_do_handshake(rdpTls* tls, BOOL clientMode)
{
	CryptoCert cert;
	int verify_status, status;

	do
	{
		struct timeval tv;
		fd_set rset;
		int fd;

		status = BIO_do_handshake(tls->bio);
		if (status == 1)
			break;
		if (!BIO_should_retry(tls->bio))
			return -1;

		/* we select() only for read even if we should test both read and write
		 * depending of what have blocked */
		FD_ZERO(&rset);

		fd = BIO_get_fd(tls->bio, NULL);
		if (fd < 0)
		{
			fprintf(stderr, "%s: unable to retrieve BIO fd\n", __FUNCTION__);
			return -1;
		}

		FD_SET(fd, &rset);
		tv.tv_sec = 0;
		tv.tv_usec = 10 * 1000; /* 10ms */

		status = select(fd + 1, &rset, NULL, NULL, &tv);
		if (status < 0)
		{
			fprintf(stderr, "%s: error during select()\n", __FUNCTION__);
			return -1;
		}
	}
	while (TRUE);

	if (!clientMode)
		return 1;

	cert = tls_get_certificate(tls, clientMode);
	if (!cert)
	{
		fprintf(stderr, "%s: tls_get_certificate failed to return the server certificate.\n", __FUNCTION__);
		return -1;
	}

	tls->Bindings = tls_get_channel_bindings(cert->px509);
	if (!tls->Bindings)
	{
		fprintf(stderr, "%s: unable to retrieve bindings\n", __FUNCTION__);
		return -1;
	}

	if (!crypto_cert_get_public_key(cert, &tls->PublicKey, &tls->PublicKeyLength))
	{
		fprintf(stderr, "%s: crypto_cert_get_public_key failed to return the server public key.\n", __FUNCTION__);
		tls_free_certificate(cert);
		return -1;
	}

	verify_status = tls_verify_certificate(tls, cert, tls->hostname, tls->port);

	if (verify_status < 1)
	{
		freerdp_log(tls->settings->instance, "%s: certificate not trusted, aborting.\n", __FUNCTION__);
		tls_disconnect(tls);
		tls_free_certificate(cert);
		return 0;
	}

	tls_free_certificate(cert);

	return verify_status;
}

int tls_connect(rdpTls* tls, BIO *underlying)
{
	int options = 0;

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif

	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	if (!tls_prepare(tls, underlying, TLSv1_client_method(), options, TRUE))
		return FALSE;

	return tls_do_handshake(tls, TRUE);
}



BOOL tls_accept(rdpTls* tls, BIO *underlying, const char* cert_file, const char* privatekey_file)
{
	long options = 0;

	/**
	 * SSL_OP_NO_SSLv2:
	 *
	 * We only want SSLv3 and TLSv1, so disable SSLv2.
	 * SSLv3 is used by, eg. Microsoft RDC for Mac OS X.
	 */
	options |= SSL_OP_NO_SSLv2;

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif
	 
	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	if (!tls_prepare(tls, underlying, SSLv23_server_method(), options, FALSE))
		return FALSE;

	if (SSL_use_RSAPrivateKey_file(tls->ssl, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "%s: SSL_CTX_use_RSAPrivateKey_file failed\n", __FUNCTION__);
		fprintf(stderr, "PrivateKeyFile: %s\n", privatekey_file);
		return FALSE;
	}

	if (SSL_use_certificate_file(tls->ssl, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "%s: SSL_use_certificate_file failed\n", __FUNCTION__);
		return FALSE;
	}

	return tls_do_handshake(tls, FALSE) > 0;
}

BOOL tls_disconnect(rdpTls* tls)
{
	if (!tls)
		return FALSE;

	if (!tls->ssl)
		return TRUE;

	if (tls->alertDescription != TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY)
	{
		/**
		 * OpenSSL doesn't really expose an API for sending a TLS alert manually.
		 *
		 * The following code disables the sending of the default "close notify"
		 * and then proceeds to force sending a custom TLS alert before shutting down.
		 *
		 * Manually sending a TLS alert is necessary in certain cases,
		 * like when server-side NLA results in an authentication failure.
		 */

		SSL_set_quiet_shutdown(tls->ssl, 1);

		if ((tls->alertLevel == TLS_ALERT_LEVEL_FATAL) && (tls->ssl->session))
			SSL_CTX_remove_session(tls->ssl->ctx, tls->ssl->session);

		tls->ssl->s3->alert_dispatch = 1;
		tls->ssl->s3->send_alert[0] = tls->alertLevel;
		tls->ssl->s3->send_alert[1] = tls->alertDescription;

		if (tls->ssl->s3->wbuf.left == 0)
			tls->ssl->method->ssl_dispatch_alert(tls->ssl);

		SSL_shutdown(tls->ssl);
	}
	else
	{
		SSL_shutdown(tls->ssl);
	}

	return TRUE;
}


BIO *findBufferedBio(BIO *front)
{
	BIO *ret = front;

	while (ret)
	{
		if (BIO_method_type(ret) == BIO_TYPE_BUFFERED)
			return ret;
		ret = ret->next_bio;
	}

	return ret;
}

int tls_write_all(rdpTls* tls, const BYTE* data, int length)
{
	int status, nchunks, commitedBytes;
	rdpTcp *tcp;
	fd_set rset, wset;
	fd_set *rsetPtr, *wsetPtr;
	struct timeval tv;
	BIO *bio = tls->bio;
	DataChunk chunks[2];

	BIO *bufferedBio = findBufferedBio(bio);
	if (!bufferedBio)
	{
		fprintf(stderr, "%s: error unable to retrieve the bufferedBio in the BIO chain\n", __FUNCTION__);
		return -1;
	}

	tcp = (rdpTcp *)bufferedBio->ptr;

	do
	{
		status = BIO_write(bio, data, length);
		/*fprintf(stderr, "%s: BIO_write(len=%d) = %d (retry=%d)\n", __FUNCTION__, length, status, BIO_should_retry(bio));*/
		if (status > 0)
			break;

		if (!BIO_should_retry(bio))
			return -1;

		/* we try to handle SSL want_read and want_write nicely */
		rsetPtr = wsetPtr = 0;
		if (tcp->writeBlocked)
		{
			wsetPtr = &wset;
			FD_ZERO(&wset);
			FD_SET(tcp->sockfd, &wset);
		}
		else if (tcp->readBlocked)
		{
			rsetPtr = &rset;
			FD_ZERO(&rset);
			FD_SET(tcp->sockfd, &rset);
		}
		else
		{
			fprintf(stderr, "%s: weird we're blocked but the underlying is not read or write blocked !\n", __FUNCTION__);
			USleep(10);
			continue;
		}

		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;

		status = select(tcp->sockfd + 1, rsetPtr, wsetPtr, NULL, &tv);
		if (status < 0)
			return -1;
	}
	while (TRUE);

	/* make sure the output buffer is empty */
	commitedBytes = 0;
	while ((nchunks = ringbuffer_peek(&tcp->xmitBuffer, chunks, ringbuffer_used(&tcp->xmitBuffer))))
	{
		int i;

		for (i = 0; i < nchunks; i++)
		{
			while (chunks[i].size)
			{
				status = BIO_write(tcp->socketBio, chunks[i].data, chunks[i].size);
				if (status > 0)
				{
					chunks[i].size -= status;
					chunks[i].data += status;
					commitedBytes += status;
					continue;
				}

				if (!BIO_should_retry(tcp->socketBio))
					goto out_fail;
				FD_ZERO(&rset);
				FD_SET(tcp->sockfd, &rset);
				tv.tv_sec = 0;
				tv.tv_usec = 100 * 1000;

				status = select(tcp->sockfd + 1, &rset, NULL, NULL, &tv);
				if (status < 0)
					goto out_fail;
			}

		}
	}

	ringbuffer_commit_read_bytes(&tcp->xmitBuffer, commitedBytes);
	return length;

out_fail:
	ringbuffer_commit_read_bytes(&tcp->xmitBuffer, commitedBytes);
	return -1;
}



int tls_set_alert_code(rdpTls* tls, int level, int description)
{
	tls->alertLevel = level;
	tls->alertDescription = description;

	return 0;
}

BOOL tls_match_hostname(char *pattern, int pattern_length, char *hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (memcmp((void*) hostname, (void*) pattern, pattern_length) == 0)
			return TRUE;
	}

	if ((pattern_length > 2) && (pattern[0] == '*') && (pattern[1] == '.') && (((int) strlen(hostname)) >= pattern_length))
	{
		char* check_hostname = &hostname[strlen(hostname) - pattern_length + 1];

		if (memcmp((void*) check_hostname, (void*) &pattern[1], pattern_length - 1) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

int tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname, int port)
{
	int match;
	int index;
	char* common_name = NULL;
	int common_name_length = 0;
	char** alt_names = NULL;
	int alt_names_count = 0;
	int* alt_names_lengths = NULL;
	BOOL certificate_status;
	BOOL hostname_match = FALSE;
	BOOL verification_status = FALSE;
	rdpCertificateData* certificate_data;

	if (tls->settings->ExternalCertificateManagement)
	{
		BIO* bio;
		int status;
		int length;
		int offset;
		BYTE* pemCert;
		freerdp* instance = (freerdp*) tls->settings->instance;

		/**
		 * Don't manage certificates internally, leave it up entirely to the external client implementation
		 */

		bio = BIO_new(BIO_s_mem());
		
		if (!bio)
		{
			fprintf(stderr, "%s: BIO_new() failure\n", __FUNCTION__);
			return -1;
		}

		status = PEM_write_bio_X509(bio, cert->px509);

		if (status < 0)
		{
			fprintf(stderr, "%s: PEM_write_bio_X509 failure: %d\n", __FUNCTION__, status);
			return -1;
		}
		
		offset = 0;
		length = 2048;
		pemCert = (BYTE*) malloc(length + 1);

		status = BIO_read(bio, pemCert, length);
		
		if (status < 0)
		{
			fprintf(stderr, "%s: failed to read certificate\n", __FUNCTION__);
			return -1;
		}
		
		offset += status;

		while (offset >= length)
		{
			length *= 2;
			pemCert = (BYTE*) realloc(pemCert, length + 1);

			status = BIO_read(bio, &pemCert[offset], length);

			if (status < 0)
				break;

			offset += status;
		}

		if (status < 0)
		{
			fprintf(stderr, "%s: failed to read certificate\n", __FUNCTION__);
			return -1;
		}
		
		length = offset;
		pemCert[length] = '\0';

		status = -1;
		
		if (instance->VerifyX509Certificate)
		{
			status = instance->VerifyX509Certificate(instance, pemCert, length, hostname, port, 0);
		}
		
		fprintf(stderr, "%s: (length = %d) status: %d\n%s\n", __FUNCTION__,	length, status, pemCert);

		free(pemCert);
		BIO_free(bio);

		if (status < 0)
			return -1;

		return (status == 0) ? 0 : 1;
	}

	/* ignore certificate verification if user explicitly required it (discouraged) */
	if (tls->settings->IgnoreCertificate)
		return 1;  /* success! */

	/* if user explicitly specified a certificate name, use it instead of the hostname */
	if (tls->settings->CertificateName)
		hostname = tls->settings->CertificateName;

	/* attempt verification using OpenSSL and the ~/.freerdp/certs certificate store */
	certificate_status = x509_verify_certificate(cert, tls->certificate_store->path);

	/* verify certificate name match */
	certificate_data = crypto_get_certificate_data(cert->px509, hostname);

	/* extra common name and alternative names */
	common_name = crypto_cert_subject_common_name(cert->px509, &common_name_length);
	alt_names = crypto_cert_subject_alt_name(cert->px509, &alt_names_count, &alt_names_lengths);

	/* compare against common name */

	if (common_name)
	{
		if (tls_match_hostname(common_name, common_name_length, hostname))
			hostname_match = TRUE;
	}

	/* compare against alternative names */

	if (alt_names)
	{
		for (index = 0; index < alt_names_count; index++)
		{
			if (tls_match_hostname(alt_names[index], alt_names_lengths[index], hostname))
			{
				hostname_match = TRUE;
				break;
			}
		}
	}

	/* if the certificate is valid and the certificate name matches, verification succeeds */
	if (certificate_status && hostname_match)
	{
		if (common_name)
		{
			free(common_name);
			common_name = NULL;
		}

		verification_status = TRUE; /* success! */
	}

	/* if the certificate is valid but the certificate name does not match, warn user, do not accept */
	if (certificate_status && !hostname_match)
		tls_print_certificate_name_mismatch_error(hostname, common_name, alt_names, alt_names_count);

	/* verification could not succeed with OpenSSL, use known_hosts file and prompt user for manual verification */

	if (!certificate_status)
	{
		char* issuer;
		char* subject;
		char* fingerprint;
		freerdp* instance = (freerdp*) tls->settings->instance;
		BOOL accept_certificate = FALSE;

		issuer = crypto_cert_issuer(cert->px509);
		subject = crypto_cert_subject(cert->px509);
		fingerprint = crypto_cert_fingerprint(cert->px509);

		/* search for matching entry in known_hosts file */
		match = certificate_data_match(tls->certificate_store, certificate_data);

		if (match == 1)
		{
			/* no entry was found in known_hosts file, prompt user for manual verification */
			if (!hostname_match)
				tls_print_certificate_name_mismatch_error(hostname, common_name, alt_names, alt_names_count);

			if (instance->VerifyCertificate)
			{
				accept_certificate = instance->VerifyCertificate(instance, subject, issuer, fingerprint);
			}

			if (!accept_certificate)
			{
				/* user did not accept, abort and do not add entry in known_hosts file */
				verification_status = FALSE; /* failure! */
			}
			else
			{
				/* user accepted certificate, add entry in known_hosts file */
				certificate_data_print(tls->certificate_store, certificate_data);
				verification_status = TRUE; /* success! */
			}
		}
		else if (match == -1)
		{
			/* entry was found in known_hosts file, but fingerprint does not match. ask user to use it */
			tls_print_certificate_error(hostname, fingerprint, tls->certificate_store->file);
			
			if (instance->VerifyChangedCertificate)
			{
				accept_certificate = instance->VerifyChangedCertificate(instance, subject, issuer, fingerprint, "");
			}

			if (!accept_certificate)
			{
				/* user did not accept, abort and do not change known_hosts file */
				verification_status = FALSE;  /* failure! */
			}
			else
			{
				/* user accepted new certificate, add replace fingerprint for this host in known_hosts file */
				certificate_data_replace(tls->certificate_store, certificate_data);
				verification_status = TRUE; /* success! */
			}
		}
		else if (match == 0)
		{
			verification_status = TRUE; /* success! */
		}

		free(issuer);
		free(subject);
		free(fingerprint);
	}

	if (certificate_data)
	{
		free(certificate_data->fingerprint);
		free(certificate_data->hostname);
		free(certificate_data);
	}

#ifndef _WIN32
	if (common_name)
		free(common_name);
#endif

	if (alt_names)
		crypto_cert_subject_alt_name_free(alt_names_count, alt_names_lengths,
				alt_names);

	return (verification_status == 0) ? 0 : 1;
}

void tls_print_certificate_error(char* hostname, char* fingerprint, char *hosts_file)
{
	fprintf(stderr, "The host key for %s has changed\n", hostname);
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @\n");
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!\n");
	fprintf(stderr, "Someone could be eavesdropping on you right now (man-in-the-middle attack)!\n");
	fprintf(stderr, "It is also possible that a host key has just been changed.\n");
	fprintf(stderr, "The fingerprint for the host key sent by the remote host is\n%s\n", fingerprint);
	fprintf(stderr, "Please contact your system administrator.\n");
	fprintf(stderr, "Add correct host key in %s to get rid of this message.\n", hosts_file);
	fprintf(stderr, "Host key for %s has changed and you have requested strict checking.\n", hostname);
	fprintf(stderr, "Host key verification failed.\n");
}

void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count)
{
	int index;

	assert(NULL != hostname);

	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "@           WARNING: CERTIFICATE NAME MISMATCH!           @\n");
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "The hostname used for this connection (%s) \n", hostname);
	fprintf(stderr, "does not match %s given in the certificate:\n", alt_names_count < 1 ? "the name" : "any of the names");
	fprintf(stderr, "Common Name (CN):\n");
	fprintf(stderr, "\t%s\n", common_name ? common_name : "no CN found in certificate");
	if (alt_names_count > 0)
	{
		assert(NULL != alt_names);
		fprintf(stderr, "Alternative names:\n");
		for (index = 0; index < alt_names_count; index++)
		{
			assert(alt_names[index]);
			fprintf(stderr, "\t %s\n", alt_names[index]);
		}
	}
	fprintf(stderr, "A valid certificate for the wrong name should NOT be trusted!\n");
}

rdpTls* tls_new(rdpSettings* settings)
{
	rdpTls* tls;

	tls = (rdpTls *)calloc(1, sizeof(rdpTls));
	if (!tls)
		return NULL;

	SSL_load_error_strings();
	SSL_library_init();

	tls->settings = settings;
	tls->certificate_store = certificate_store_new(settings);
	if (!tls->certificate_store)
		goto out_free;

	tls->alertLevel = TLS_ALERT_LEVEL_WARNING;
	tls->alertDescription = TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY;
	return tls;

out_free:
	free(tls);
	return NULL;
}

void tls_free(rdpTls* tls)
{
	if (!tls)
		return;

	if (tls->ctx)
	{
		SSL_CTX_free(tls->ctx);
		tls->ctx = NULL;
	}

	if (tls->PublicKey)
	{
		free(tls->PublicKey);
		tls->PublicKey = NULL;
	}

	if (tls->Bindings)
	{
		free(tls->Bindings->Bindings);
		free(tls->Bindings);
		tls->Bindings = NULL;
	}

	certificate_store_free(tls->certificate_store);
	tls->certificate_store = NULL;

	free(tls);
}
