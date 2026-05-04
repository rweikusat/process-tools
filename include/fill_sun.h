/*
  initialize a sockaddr_un structure
*/
#ifndef proc_tools_fill_sun_h
#define proc_tools_fill_sun_h

/*  types */
struct sockaddr_un;

/*  routines */
void fill_sun(char *addr,
              struct sockaddr_un *sun, unsigned *sun_len);

#endif
