// Put this in a separate .h file (called "getopt.h").
// The prototype for the header file is:

#ifndef GETOPT_H
#define GETOPT_H
 
extern int optind;
extern char *optarg;
extern int getopt(int nargc, char * const nargv[], const char *ostr) ;
 
#endif
