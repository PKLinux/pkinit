#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/reboot.h>

#define INIT_DAWN "/etc/init/dawn/all"
#define INIT_DUSK "/etc/init/dusk/all"

void on_reboot(int sig);
void on_shutdown(int sig);
void on_halt(int sig);
void dawn(void);
void dusk(void);

void on_reboot(int sig) {
	printf("init: reboot queried.\n");
	dusk();
	reboot(LINUX_REBOOT_CMD_RESTART);
}

void on_shutdown(int sig) {
	printf("init: shutdown queried.\n");
	dusk();
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

void on_halt(int sig) {
	printf("init: halt queried.\n");
	sync();
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
}


void dawn() {
	system(INIT_DAWN);
}

void dusk() {
	system(INIT_DUSK);
	sync();
}

int main() {
	if (getpid() != 1) {
		fprintf(stderr, "init: not run as pid eins.\n");
		return 1;
	}
	
	signal(SIGUSR2, on_reboot);
	signal(SIGUSR1, on_shutdown);
	signal(SIGINT, on_halt);

	dawn();

	while (1)
		waitpid(-1, NULL, 0);
}
