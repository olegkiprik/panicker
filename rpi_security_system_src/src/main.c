#include <stdatomic.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <periphery/gpio.h>

#define NR_BITS 4
#define BASE_TIMEOUT_MS 10
#define BOUNCE_TIMEOUT_MS 300

#define pwd_size 7

const int gpio_in_nr[4] = { 18,17,10,25 };
const int gpio_out_nr[4] = { 24,27,23,22 };

gpio_t* gpio_in[4] = {0};
gpio_t* gpio_out[4] = {0};

gpio_t* gpio_guard_in;
gpio_t* gpio_guard_out;

pthread_t guard_tid;
volatile atomic_int_fast8_t guard_alarming = 0;

enum State {
	STATE_AWAITING,
	STATE_PASSWORD,
	STATE_CALM,
	STATE_ALARM
};

void free_all() {
	gpio_close(gpio_guard_in);
	gpio_free(gpio_guard_in);
	gpio_close(gpio_guard_out);
	gpio_free(gpio_guard_out);
	for (int i = 0; i < NR_BITS; ++i) {
		gpio_close(gpio_in[i]);
		gpio_free(gpio_in[i]);
		gpio_close(gpio_out[i]);
		gpio_free(gpio_out[i]);
	}
	if (guard_alarming) {
		guard_alarming = 0;
		pthread_join(guard_tid, NULL);
	}
}

typedef struct timespec timespec_t;

void microsleep(uint_least32_t microsec)
{
    time_t sec = (int)(microsec / 1000000);
    microsec = microsec - (sec * 1000000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = microsec * 1000L;
    if (nanosleep(&req, &req))
    {
		fprintf(stderr, "nanosleep error line %d\n", __LINE__);
		return;
	}
}

void* guard_work(void* args) {
	clock_t c0 = clock();
	bool up = 0;
	for (;;) {
		microsleep(500 * (2 + sin(clock() * 100. / CLOCKS_PER_SEC)));
		if ((clock() - c0) * 1000. / CLOCKS_PER_SEC > 10) {
			if (!guard_alarming) {
				return NULL;
			}
			c0 = clock();
		}
		up = !up;
		if (gpio_write(gpio_guard_out, up) < 0) {
			fprintf(stderr, "error line %d\n", __LINE__);
    	}
	}
	return NULL;
}

int update_diodes(const int* arr) {
	for (int i = 0; i < NR_BITS; ++i) {
		if (0 > gpio_write(gpio_out[i], arr[i])) {
			fprintf(stderr, "gpio_write(): %s\n", gpio_errmsg(gpio_out[i]));
			return 0;
		}
	}
	return 1;
}

int pressed() {
	uint64_t timestamp;
	gpio_edge_t edge;
	bool gpios_ready[4];
	bool gpios_ready_2[4];
	int poll_value = gpio_poll_multiple(gpio_in, NR_BITS, BASE_TIMEOUT_MS, gpios_ready_2);
	for (int i = 0; i < NR_BITS; ++i) {
		gpios_ready[i] = gpios_ready_2[i];
	}
	int return_value = 0;
	if (poll_value < 0) {
		fprintf(stderr, "gpio_poll_multiple(): error\n");
		return -1;
	}
	if (poll_value > 0) {
		/* bounce effect processing */
		clock_t c0 = clock();

		for (int k = 0; k < NR_BITS; ++k) {
				if (gpios_ready_2[k]) {
					if (gpio_read_event(gpio_in[k], &edge, &timestamp) < 0) {
						fprintf(stderr, "gpio_read(): %s\n", gpio_errmsg(gpio_in[k]));
						return -1;
					}
				}
			}

		for (;;) {
			if ((clock() - c0) * 100. / CLOCKS_PER_SEC > BOUNCE_TIMEOUT_MS) {
				break;
			}

			int poll_value2 = gpio_poll_multiple(gpio_in, NR_BITS, BOUNCE_TIMEOUT_MS, gpios_ready_2);
			if (poll_value2 < 0) {
				fprintf(stderr, "gpio_poll_multiple(): error\n");
				return -1;
			} else if (poll_value2 > 0) {
				for (int k = 0; k < NR_BITS; ++k) {
					if (gpios_ready_2[k]) {
						if (gpio_read_event(gpio_in[k], &edge, &timestamp) < 0) {
							fprintf(stderr, "gpio_read(): %s\n", gpio_errmsg(gpio_in[k]));
							return -1;
						}
					}
				}
				c0 = clock();
			} else {
				break;
			}
		}
		for (int k = 0; k < NR_BITS; ++k) {
			if (gpios_ready[k]) {
				return_value = k + 1;
			}
		}
	}

	return return_value;
}

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = read(fd, buf, count);
        if (c < 0)
            return c;
        if (0 == c)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = write(fd, buf, count);
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}


int make_tcp_socket(void)
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
		return -1;
	}
    return sock;
}

struct sockaddr_in make_address(char *address, char *port)
{
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    if ((ret = getaddrinfo(address, port, &hints, &result)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in *)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

int connect_tcp_socket(char *name, char *port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
		return -1;
	}
    return socketfd;
}

char* ipaddr;
void guard_alarm() {
	int fd;
    int32_t data[2];
    fd = connect_tcp_socket(ipaddr, "10000");
	if (fd < 0) {
		fprintf(stderr, "Not connected 1!\n");
		return;
	}
	if (bulk_write(fd, (char *)data, sizeof(int32_t[2])) < 0) {
		fprintf(stderr, "Not connected 2!\n");
		return;
	}
	close(fd);
}

int main(int argc, char** argv) {
    if (argc < 2) {
    	fprintf(stderr, "USAGE: %s host_ip_address\n", argv[0]);
	return EXIT_FAILURE;
    }
    else {
    	ipaddr = argv[1];
    }

	gpio_guard_in = gpio_new();
	gpio_guard_out = gpio_new();

    if (gpio_open(gpio_guard_in, "/dev/gpiochip0", 26, GPIO_DIR_IN) < 0) {
        fprintf(stderr, "gpio_open(): %s\n", gpio_errmsg(gpio_guard_in));
        return EXIT_FAILURE;
    }

    if (gpio_open(gpio_guard_out, "/dev/gpiochip0", 21, GPIO_DIR_OUT) < 0) {
        fprintf(stderr, "gpio_open(): %s\n", gpio_errmsg(gpio_guard_out));
        gpio_close(gpio_guard_in);
		gpio_free(gpio_guard_in);
		return EXIT_FAILURE;
    }
	
	for (int i = 0; i < NR_BITS; ++i) {
		gpio_in[i] = gpio_new();
		if (gpio_open(gpio_in[i], "/dev/gpiochip0", gpio_in_nr[i], GPIO_DIR_IN) < 0) {
			fprintf(stderr, "gpio_open(): %s\n", gpio_errmsg(gpio_in[i]));
			gpio_close(gpio_guard_in);
			gpio_free(gpio_guard_in);
			gpio_close(gpio_guard_out);
			gpio_free(gpio_guard_out);
			for (int j = 0; j < i; ++j) {
				gpio_close(gpio_in[j]);
				gpio_free(gpio_in[j]);
			}
			return EXIT_FAILURE;
		}
		gpio_set_edge(gpio_in[i], GPIO_EDGE_FALLING);
	}

	for (int i = 0; i < NR_BITS; ++i) {
		gpio_out[i] = gpio_new();
		if (gpio_open(gpio_out[i], "/dev/gpiochip0", gpio_out_nr[i], GPIO_DIR_OUT) < 0) {
			fprintf(stderr, "gpio_open(): %s\n", gpio_errmsg(gpio_out[i]));
			gpio_close(gpio_guard_in);
			gpio_free(gpio_guard_in);
			gpio_close(gpio_guard_out);
			gpio_free(gpio_guard_out);
			for (int j = 0; j < i; ++j) {
				gpio_close(gpio_out[j]);
				gpio_free(gpio_out[j]);
			}
			for (int j = 0; j < NR_BITS; ++j) {
				gpio_close(gpio_in[j]);
				gpio_free(gpio_in[j]);
			}
			return EXIT_FAILURE;
		}
	}

	for (int i = 0; i < NR_BITS; ++i) {
		if (0 > gpio_write(gpio_out[i], false)) {
			fprintf(stderr, "gpio_write(): %s\n", gpio_errmsg(gpio_out[i]));
			free_all();
			return EXIT_FAILURE;
		}
	}

	enum State state = STATE_AWAITING;
	clock_t password_clock;

	
	const int password[pwd_size] = {0,1,2,3,2,1,0};
	int pwd_i = 0;
	int pwd_correct = 1;
	int pwd_attempt = 0;
	const int pwd_max_attempts = 4;

	for (;;) {

			bool watch_value = 0;
			if (gpio_read(gpio_guard_in, &watch_value) < 0) {
				free_all();
				return EXIT_FAILURE;
			}

			if (watch_value && state == STATE_AWAITING) {
				if (!guard_alarming) {
					guard_alarming = 1;
					if (pthread_create(&guard_tid, NULL, guard_work, NULL)) {
						guard_alarming = 0;
					}
					else {
						state = STATE_PASSWORD;
						password_clock = clock();
						const int tmp_diodes[4] = {0, 1, 0, 0};
						update_diodes(tmp_diodes);
					}
				}
			}

			if (state == STATE_PASSWORD && (clock() - password_clock) * 1000. / CLOCKS_PER_SEC > 1000) {
				if (1 || watch_value) {
					state = STATE_ALARM;
					guard_alarm();
				} else {
					state = STATE_AWAITING;
					pwd_i = 0;
					pwd_attempt = 0;
					pwd_correct = 1;
					const int tmp_diodes[4] = {0, 0, 0, 0};
					update_diodes(tmp_diodes);
					if (guard_alarming) {
						guard_alarming = 0;
						pthread_join(guard_tid, NULL);
					}
				}
			}

			int i = pressed();
			if (i < 0) {
				free_all();
				return EXIT_FAILURE;
			}
			if (i > 0) {
				--i;

				if (state == STATE_PASSWORD) {
					const int tmp_diodes[4] = {0, 1, 0, 0};
					update_diodes(tmp_diodes);
					if (i == password[pwd_i]) {
						++pwd_i;
						if (pwd_i == pwd_size && pwd_correct) {
							state = STATE_CALM;
							const int tmp_diodes[4] = {0, 0, 0, 1};
							update_diodes(tmp_diodes);
							if (guard_alarming) {
								guard_alarming = 0;
								pthread_join(guard_tid, NULL);
							}
						}
					}
					else {
						++pwd_i;
						pwd_correct = 0;
						if (pwd_i == pwd_size) {
							++pwd_attempt;
							if (pwd_max_attempts == pwd_attempt) {
								state = STATE_ALARM;
								guard_alarm();
							} else {
								const int tmp_diodes[4] = {0, 1, 1, 0};
								update_diodes(tmp_diodes);
								pwd_i = 0;
								pwd_correct = 1;
							}
						}
					}
				} else if (state == STATE_ALARM || state == STATE_CALM) {
					state = STATE_AWAITING;
					pwd_i = 0;
					pwd_attempt = 0;
					pwd_correct = 1;
					const int tmp_diodes[4] = {0, 0, 0, 0};
					update_diodes(tmp_diodes);
					if (guard_alarming) {
						guard_alarming = 0;
						pthread_join(guard_tid, NULL);
					}
				} else if (state == STATE_AWAITING) {
					break;
				}
			}
	}

	free_all();
	return EXIT_SUCCESS;
}
