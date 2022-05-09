#include <stdbool.h>
#include <alsa/asoundlib.h>

#define LOCK_TIMEOUT 10

extern int debugflag;
extern int force_restore;
extern int ignore_nocards;
extern int do_lock;
extern int use_syslog;
extern char *command;
extern char *statefile;
extern char *lockpath;
extern char *lockfile;

struct snd_card_iterator {
	int card;
	char name[16];
	bool single;
	bool first;
};

void info_(const char *fcn, long line, const char *fmt, ...);
void error_(const char *fcn, long line, const char *fmt, ...);
void cerror_(const char *fcn, long line, int cond, const char *fmt, ...);
void dbg_(const char *fcn, long line, const char *fmt, ...);
void error_handler(const char *file, int line, const char *function, int err, const char *fmt, ...);

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define info(...) do { info_(__func__, __LINE__, __VA_ARGS__); } while (0)
#define error(...) do { error_(__func__, __LINE__, __VA_ARGS__); } while (0)
#define cerror(cond, ...) do { cerror_(__func__, __LINE__, (cond) != 0, __VA_ARGS__); } while (0)
#define dbg(...) do { dbg_(__func__, __LINE__, __VA_ARGS__); } while (0)
#else
#define info(args...) do { info_(__func__, __LINE__, ##args); }  while (0)
#define error(args...) do { error_(__func__, __LINE__, ##args); }  while (0)
#define cerror(cond, ...) do { error_(__func__, __LINE__, (cond) != 0, ##args); } while (0)
#define dbg(args...) do { dbg_(__func__, __LINE__, ##args); }  while (0)
#endif	

#define FLAG_UCM_DISABLED	(1<<0)
#define FLAG_UCM_FBOOT		(1<<1)
#define FLAG_UCM_BOOT		(1<<2)
#define FLAG_UCM_DEFAULTS	(1<<3)
#define FLAG_UCM_NODEV		(1<<4)

void snd_card_iterator_init(struct snd_card_iterator *iter, int cardno);
int snd_card_iterator_sinit(struct snd_card_iterator *iter, const char *cardname);
const char *snd_card_iterator_next(struct snd_card_iterator *iter);
int snd_card_iterator_error(struct snd_card_iterator *iter);

int load_configuration(const char *file, snd_config_t **top, int *open_failed);
int init(const char *cfgdir, const char *file, int flags, const char *cardname);
int init_ucm(int flags, int cardno);
int state_lock(const char *file, int timeout);
int state_unlock(int lock_fd, const char *file);
int card_lock(int card_number, int timeout);
int card_unlock(int lock_fd, int card_number);
int save_state(const char *file, const char *cardname);
int load_state(const char *cfgdir, const char *file,
	       const char *initfile, int initflags,
	       const char *cardname, int do_init);
int power(const char *argv[], int argc);
int monitor(const char *name);
int general_info(const char *name);
int state_daemon(const char *file, const char *cardname, int period,
		 const char *pidfile);
int state_daemon_kill(const char *pidfile, const char *cmd);
int clean(const char *cardname, char *const *extra_args);
int snd_card_clean_cfgdir(const char *cfgdir, int cardno);

/* utils */

int file_map(const char *filename, char **buf, size_t *bufsize);
void file_unmap(void *buf, size_t bufsize);
size_t line_width(const char *buf, size_t bufsize, size_t pos);
void initfailed(int cardnumber, const char *reason, int exitcode);

static inline int hextodigit(int c)
{
        if (c >= '0' && c <= '9')
                c -= '0';
        else if (c >= 'a' && c <= 'f')
                c = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
                c = c - 'A' + 10;
        else
                return -1;
        return c;
}

#define ARRAY_SIZE(a) (sizeof (a) / sizeof (a)[0])
