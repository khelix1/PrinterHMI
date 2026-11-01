/* Desktop and firmware demo share the same local backend and actual Files UI. */
#include "ui_demo_files.h"
void host_files_close(void){ui_demo_files_close();}
void host_files_show(bool (*send)(const char *)){ui_demo_files_show(send);}
