#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/reboot.h>

#define HOSTNAME "/etc/hostname"
#define HOSTNAME_KERNEL "/proc/sys/kernel/hostname"
#define ENTROPY_VAULT "/var/entropy"
#define ENTROPY_AMOUNT 2048
#define RANDOM "/dev/random"
#define SERVICE "/etc/init/service"

enum sht_type {
	SHT_NONE,
	SHT_REBOOT,
	SHT_SHUTDOWN,
} sht = SHT_NONE;

pid_t service = -1;

void on_reboot(int sig);
void on_shutdown(int sig);
void on_halt(int sig);
void mounts(void);
void exchange_bytes(char *path1, char *path2, size_t amount);
void dawn(void);
void dusk(void);
int emergency_shell(void);
int start_service_manager(void);

/* on_reboot(): set sht type to reboot and communicates to the service manager */
void on_reboot(int sig) {
	printf("init: reboot queried.\n");
	sht = SHT_REBOOT;
	kill(service, sig);
}

/* on_shutdown(): set the sht type to halt and communicates to the service manager */
void on_shutdown(int sig) {
	printf("init: shutdown queried.\n");
	sht = SHT_SHUTDOWN;
	kill(service, sig);
}

/* on_halt(): halts the system */
void on_halt(int sig) {
	printf("init: halt queried.\n");
	dawn();
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

/* mounts(): mounts filesystems */
void mounts() {
	mount("proc", "/proc", "proc", 0x4|0x2|0x8, "");
	mount("sys", "/sys", "sysfs", 0x4|0x2|0x8, "");
	mount("run", "/run", "tmpfs", 0x4|0x2, "mode=0755");
	mount("dev", "/dev", "devtmpfs", 0x2, "mode=0755");
}

/* exchange_bytes(): preserves amount of bytes from file at path1 into file at path2 */
void exchange_bytes(char *path1, char *path2, size_t amount) {
	FILE *file1 = fopen(path1, "r");
	if (file1 == NULL) {
		fprintf(stderr, "init: warning: cannot open %s\n", path1);
		return;
	}

	FILE *file2 = fopen(path2, "w+");
	if (file2 == NULL) {
		fprintf(stderr, "init: warning: cannot open %s\n", path2);
		goto close_file1;
	}
	
	char *data = malloc(amount);
	if (data == NULL)
		goto close_file2;
	
	ssize_t ret = read(fileno(file1), data, ENTROPY_AMOUNT);
	if (ret <= 0)
		goto free_data;

	write(fileno(file2), data, ret);

free_data:
	free(data);
close_file2:
	fclose(file2);
close_file1:
	fclose(file1);
}

void dawn() {
	mounts();
	exchange_bytes(ENTROPY_VAULT, RANDOM, ENTROPY_AMOUNT);
}

void dusk() {
	exchange_bytes(RANDOM, ENTROPY_VAULT, ENTROPY_AMOUNT);
	sync();
}

int emergency_shell() {
	printf("init: spawning an emergency shell; the system will reboot when you exit\n");
	system("/bin/sh");
	dusk();
	return reboot(LINUX_REBOOT_CMD_RESTART);
}

int start_service_manager() {
	char *argv[1] = {NULL};
	char *envp[1] = {NULL};
	if (execve(SERVICE, argv, envp) == -1) {
		fprintf(stderr, "init: error: could not spawn the service manager at %s; please configure your system accordingly.\n", SERVICE);
		return emergency_shell();
	}
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

service:
	if ((service = fork()) == 0)
		return start_service_manager();
	else if (service == -1) {
		fprintf(stderr, "init: fatal: could not fork the service process; attempting an emergency shell\n");
		return emergency_shell();
	}

	while (1) {
		pid_t proc = waitpid(-1, NULL, 0);
		if (proc != service)
			continue;

		switch (sht) {
		case SHT_NONE:
			goto service;
		case SHT_SHUTDOWN:
			dusk();
			return reboot(LINUX_REBOOT_CMD_POWER_OFF);
		case SHT_REBOOT:
			dusk();
			return reboot(LINUX_REBOOT_CMD_RESTART);
		}
	}
}
