#include <sys/types.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "linenoise.h"

#define HISTFILE ".shell_history"
#define HISTFILESIZE 1000

struct command {
	const char *name, *usage, *descr;
	void(*handler)(struct command *, char **, size_t);
	bool(*complete)(char **, size_t, linenoiseCompletions *);
};

extern struct command cmds[];

struct command *findcom (const char *name)
{
	struct command *cmd;

	for (cmd = cmds; cmd->name != NULL; ++cmd) {
		if (strcmp (name, cmd->name) == 0)
			return cmd;
	}

	return NULL;
}

void usage (const struct command *cmd)
{
	printf ("usage: %s\n", cmd->usage);
}

void cmd_echo (struct command *cmd, char **args, size_t num)
{
	(void)cmd;

	if (num > 1) {
		printf ("%s", args[1]);
		for (size_t i = 2; i < num; ++i) {
			printf (" %s", args[i]);
		}
	}
	putchar ('\n');
}

void cmd_ls (struct command *cmd, char **args, size_t num)
{
	DIR *dir;
	struct dirent *ent;
	const char *path;
	char type;

	switch (num) {
	case 1:
		path = ".";
		break;
	case 2:
		path = args[1];
		break;
	default:
		usage (cmd);
		return;
	}

	dir = opendir (path);
	if (dir == NULL) {
		perror ("opendir()");
		return;
	}

	while ((ent = readdir (dir)) != NULL) {
		if (strcmp (ent->d_name, ".") == 0 || strcmp (ent->d_name, "..") == 0)
			continue;

		switch (ent->d_type) {
		case DT_BLK:
			type = 'b';
			break;
		case DT_CHR:
			type = 'c';
			break;
		case DT_DIR:
			type = 'd';
			break;
		case DT_FIFO:
			type = 'f';
			break;
		case DT_LNK:
			type = 'l';
			break;
		case DT_REG:
			type = 'f';
			break;
		case DT_SOCK:
			type = 's';
			break;
		default:
			type = '?';
		}
		printf ("%c %s\n", type, ent->d_name);
	}

	closedir (dir);
}

void cmd_pwd (struct command *cmd, char **args, size_t num)
{
	char cwd[PATH_MAX];
	(void)args;

	if (num != 1) {
		usage (cmd);
		return;
	}

	if (getcwd (cwd, sizeof (cwd)) == NULL) {
		perror ("getcwd()");
		return;
	}

	puts (cwd);
}

void cmd_cd (struct command *cmd, char **args, size_t num)
{
	const char *path;

	switch (num) {
	case 1:
		path = getenv ("HOME");
		if (path == NULL)
			path = "/";
		break;
	case 2:
		path = args[1];
		break;
	default:
		usage (cmd);
		return;
	}

	if (chdir (path) != 0)
		perror ("chdir()");
}

void cmd_cat (struct command *cmd, char **args, size_t num)
{
	const char *path;
	char buf[128];
	FILE *file;
	ssize_t n;

	if (num <= 1) {
		usage (cmd);
		return;
	}

	for (size_t i = 1; i < num; ++i) {
		path = args[i];
		file = fopen (path, "r");
		if (file == NULL) {
			printf ("error: fopen('%s'): %s\n", path, strerror (errno));
			continue;
		}

		while ((n = fread (buf, 1, sizeof (buf), file)) > 0) {
			fwrite (buf, 1, n, stdout);
		}

		fclose (file);
	}
}

void cmd_help (struct command *cmd, char **args, size_t num)
{
	switch (num) {
	case 1:
		for (cmd = cmds; cmd->name != NULL; ++cmd)
			printf ("%-30s- %s\n", cmd->usage, cmd->descr);
		break;
	case 2:
		cmd = findcom (args[1]);
		if (cmd == NULL) {
			printf ("Invalid command: %s\n", args[1]);
			return;
		}
		
		// fallthrough
	default:
		usage (cmd);
		break;
	}
}

char *join (char **args, size_t num)
{
	static char buf[256];
	char *s = buf;

	strcpy (s, args[0]);
	for (size_t i = 1; i < num; ++i) {
		strcat (s, " ");
		strcat (s, args[i]);
	}

	return buf;
}

void cmd_history (struct command *cmd, char **args, size_t num)
{
	FILE *file;
	char *line = NULL;
	size_t cap = 0;

	(void)args;

	if (num != 1) {
		usage (cmd);
		return;
	}

	linenoiseHistorySave (HISTFILE);

	file = fopen (HISTFILE, "r");
	if (file == NULL)
		return;

	for (size_t i = 1; getline (&line, &cap, file) > 0; ++i) {
		printf ("%-4zu %s", i, line);
	}

	free (line);
	fclose (file);
}

void cmd_clear (struct command *cmd, char **args, size_t num)
{
	(void)args;

	if (num != 1) {
		usage (cmd);
		return;
	}

	linenoiseClearScreen ();
}

void cmd_keys (struct command *cmd, char **args, size_t num)
{
	(void)args;

	if (num != 1) {
		usage (cmd);
		return;
	}

	linenoisePrintKeyCodes ();
}

void cmd_mask (struct command *cmd, char **args, size_t num)
{
	char *arg;
	
	if (num != 2) {
		usage (cmd);
		return;
	}

	arg = args[1];
	if (strcmp (arg, "on") == 0) {
		linenoiseMaskModeEnable ();
	} else if (strcmp (arg, "off") == 0) {
		linenoiseMaskModeDisable ();
	} else {
		usage (cmd);
	}
}

void cmd_multiline (struct command *cmd, char **args, size_t num)
{
	char *arg;
	
	if (num != 2) {
		usage (cmd);
		return;
	}

	arg = args[1];
	if (strcmp (arg, "on") == 0) {
		linenoiseSetMultiLine (1);
	} else if (strcmp (arg, "off") == 0) {
		linenoiseSetMultiLine (0);
	} else {
		usage (cmd);
	}
}
void cmd_exit (struct command *cmd, char **args, size_t num)
{
	(void)cmd;
	(void)args;
	(void)num;

	linenoiseHistorySave (HISTFILE);
	exit (0);
}

bool cpl_none (char **args, size_t num, linenoiseCompletions *c)
{
	(void)args;
	(void)num;
	(void)c;

	return false;
}

bool cpl_mask (char **args, size_t num, linenoiseCompletions *c)
{
	char *arg;
	if (num != 2)
		return false;

	arg = args[1];
	if (arg[0] == '\0')
		goto all;

	if (arg[0] != 'o')
		return false;

	switch (arg[1]) {
	case '\0':
	all:
		linenoiseAddCompletion (c, "mask on");
		linenoiseAddCompletion (c, "mask off");
		return true;
	case 'n':
		linenoiseAddCompletion (c, "mask on");
		return true;
	case 'f':
		linenoiseAddCompletion (c, "mask off");
		return true;
	default:
		return false;
	}
}

bool cpl_files (char **args, size_t num, linenoiseCompletions *c, bool fdir)
{
	DIR *dir;
	struct dirent *ent;
	char *arg, *bn, *dn, *dp, *tmp, *compl;
	size_t bnlen;
	bool success = false;

	if (num > 2)
		return false;

	arg = args[num - 1];
	tmp = strrchr (arg, '/');

	if (tmp != NULL) {
		bn = strdup (tmp + 1);
		dp = dn = strndup (arg, tmp - arg + 1);
	} else {
		bn = strdup (arg);
		dn = strdup ("");
		dp = ".";
	}
	bnlen = strlen (bn);

	dir = opendir (dp);
	if (dir == NULL)
		return false;

	while ((ent = readdir (dir)) != NULL) {
		if (strncmp (ent->d_name, bn, bnlen) != 0)
			continue;
		if (strcmp (ent->d_name, ".") == 0 || strcmp (ent->d_name, "..") == 0)
			continue;
		if (fdir && ent->d_type != DT_DIR)
			continue;

		compl = join (args, num - 1);
		strcat (compl, " ");
		strcat (compl, dn);
		strcat (compl, ent->d_name);
		if (ent->d_type == DT_DIR)
			strcat (compl, "/");
		linenoiseAddCompletion (c, compl);
		success = true;
	}

	closedir (dir);
	free (bn);
	free (dn);
	
	return success;
}

bool cpl_ls (char **args, size_t num, linenoiseCompletions *c)
{
	return cpl_files (args, num, c, true);
}

bool cpl_cat (char **args, size_t num, linenoiseCompletions *c)
{
	return cpl_files (args, num, c, false);
}

bool cpl_help (char **args, size_t num, linenoiseCompletions *c)
{
	struct command *cmd;
	const char *prefix;
	char *compl;
	size_t len;
	bool success = false;

	if (num != 2)
		return false;

	prefix = args[1];
	len = strlen (prefix);
	for (cmd = cmds; cmd->name != NULL; ++cmd) {
		if (strncmp (prefix, cmd->name, len) != 0)
			continue;
		
		asprintf (&compl, "%s %s", args[0], cmd->name);
		linenoiseAddCompletion (c, compl);
		free (compl);
		success = true;
	}

	return success;
}

#define CMD(nm, us, ds, cpl) \
{ .name = #nm, .usage = #nm us, .descr = ds, .handler = cmd_##nm, .complete = cpl }
struct command cmds[] = {
	CMD (echo,	" string...",	"print text",			cpl_none),
	CMD (ls,	" [path]",	"list files",			cpl_ls),
	CMD (pwd,	"",		"print working directory",	cpl_none),
	CMD (cd,	" [path]",	"change directory",		cpl_ls),
	CMD (cat,	" file...",	"show files",			cpl_cat),
	CMD (help,	" [command]",	"get help",			cpl_help),
	CMD (history,	"",		"show history",			cpl_none),
	CMD (clear,	"",		"clear screen",			cpl_none),
	CMD (keys,	"",		"show keys",			cpl_none),
	CMD (mask,	" on|off",	"set mask mode",		cpl_mask),
	CMD (multiline,	" on|off",	"multiline mode",		cpl_mask),
	CMD (exit,	"",		"bye bye",			cpl_none),
	{ .name = NULL },
};

void split_args (char *line, char ***args, size_t *len)
{
	char *arg;
	size_t cap;

	cap = 5;
	*len = 0;
	*args = calloc (cap + 2, sizeof (char *));

	while ((arg = strsep (&line, " \t")) != NULL) {
		if (*arg == '\0')
			continue;

		if (*len == cap) {
			cap *= 2;
			*args = reallocarray (*args, cap + 2, sizeof (char *));
		}
		(*args)[(*len)++] = strdup (arg);
	}

	(*args)[*len] = NULL;
}

void free_args (char **args, size_t len)
{
	for (size_t i = 0; i < len; ++i)
		free (args[i]);
	free (args);
}

void runcom (char *line)
{
	struct command *cmd;
	char **args;
	size_t len;

	split_args (line, &args, &len);
	
	if (len == 0)
		goto ret;

	cmd = findcom (args[0]);
	if (cmd == NULL) {
		printf ("Invalid command: %s\n", args[0]);
		goto ret;
	}

	cmd->handler (cmd, args, len);

ret:
	free_args (args, len);
}

void complete (const char *s, linenoiseCompletions *c)
{
	struct command *cmd;
	char *line, **args, *prefix;
	size_t len, slen, plen;
	bool success;

	line = strdup (s);
	split_args (line, &args, &len);

	slen = strlen (s);
	if (slen > 0 && isspace (s[slen - 1])) {
		args[len++] = strdup ("");
		args[len] = NULL;
	}

	switch (len) {
	case 0:
		prefix = "";
		goto cpl_cmd;
	case 1:
		prefix = args[0];
	cpl_cmd:
		plen = strlen (prefix);
		success = false;
		for (cmd = cmds; cmd->name != NULL; ++cmd) {
			if (strncmp (prefix, cmd->name, plen) == 0) {
				linenoiseAddCompletion (c, cmd->name);
				success = true;
			}
		}
		break;
	default:
		cmd = findcom (args[0]);
		if (cmd == NULL)
			goto fail;

		success = cmd->complete (args, len, c);
		break;
	}

	if (!success) {
	fail:
		linenoiseAddCompletion (c, join (args, len));
	}

	free (line);
	free_args (args, len);
}

const char *prompt = "$ ";
void shell_sync (void)
{
	char *line;

	while ((line = linenoise (prompt)) != NULL) {
		linenoiseHistoryAdd (line);

		runcom (line);
		
		linenoiseFree (line);
	}
}

static struct linenoiseState ls;
void handle_sig (int sig)
{
	linenoiseHide (&ls);
	printf ("signal received: %s\n", strsignal (sig));
	linenoiseShow (&ls);
}

void shell_async (void)
{
	struct timeval tv;
	char buf[1024], *line;
	fd_set fds;
	int ret;

	signal (SIGUSR1, handle_sig);
	signal (SIGUSR2, handle_sig);

	while (1) {
		linenoiseEditStart (&ls, -1, -1, buf, sizeof (buf), prompt);

		while (1) {
			FD_ZERO (&fds);
			FD_SET (ls.ifd, &fds);
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			ret = select (ls.ifd + 1, &fds, NULL, NULL, &tv);

			if (ret < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				perror ("select()");
				exit (1);
			}

			if (ret == 0) {
				continue;
			}

			if (FD_ISSET (ls.ifd, &fds)) {
				line = linenoiseEditFeed (&ls);
				if (line != linenoiseEditMore)
					break;
			}
		}

		linenoiseEditStop (&ls);
		if (line == NULL)
			return;
		runcom (line);
	}
}

int main (int argc, char *argv[])
{
	int option;
	bool async = false;

	while ((option = getopt (argc, argv, "a")) != -1) {
		switch (option) {
		case 'a':
			async = true;
			break;
		default:
			fprintf (stderr, "usage: shell [-a]\n");
			return 1;
		}
	}

	linenoiseHistorySetMaxLen (HISTFILESIZE);
	linenoiseHistoryLoad (HISTFILE);
	linenoiseSetCompletionCallback (complete);

	if (async) {
		shell_async ();
	} else {
		shell_sync ();
	}

	linenoiseHistorySave (HISTFILE);

	return 0;
}
