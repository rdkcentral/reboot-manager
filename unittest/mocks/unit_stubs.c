#include <stdarg.h>
#include <stdbool.h>

/* Test-local C stubs for symbols referenced by rebootNow_main.c */
int pidfile_write_and_guard(void){ return 0; }
void cleanup_pidfile(void){}

bool rbus_init(void){ return true; }
void rbus_cleanup(void){}

bool rbus_get_bool_param(const char* name, bool* value){ (void)name; if(value) *value=false; return true; }
bool rbus_set_int_param(const char* name, int value){ (void)name; (void)value; return true; }

int handle_cyclic_reboot(const char* s, const char* r, const char* c, const char* o){ (void)s; (void)r; (void)c; (void)o; return 0; }

void cleanup_services(void){}
