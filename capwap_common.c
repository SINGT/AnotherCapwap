#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "CWLog.h"

static void strip_ret(char *s)
{
	while (*s != '\0') {
		if (*s == '\n') {
			*s = '\0';
			return;
		}
		s++;
	}
}

static int get_file_string(const char *file_name, const char *fallback_command, char *buff, int len)
{
	FILE *fstream = NULL;
	char command[256];

	if ((!file_name) || (!buff) || (len == 0))
		return -EINVAL;

	snprintf(command, sizeof(command),
		 "cat %s | awk -F \"\" '{for(i=1; i<=NF; i++){ if (($i <= \"z\") && ( $i >= \"!\" ) && (length($i) == 1)) a=a$i }} END{print a}'",
		 file_name);
	fstream = popen(command, "r");
	if ((!fstream) && fallback_command) {
		snprintf(command, sizeof(command),
			 "%s  | awk '{for(i=2; i<=NF; i++){ if (($i <= \"z\") && ( $i >= \"!\" ) && (length($i) == 1)) a=a$i }} END{print a}'",
			 fallback_command);
		fstream = popen(command, "r");
	}
	if (!fstream)
		return -EIO;

	while (fgets(buff, len, fstream) != NULL)
		;

	if (buff[0] == '\n')
		return -EINVAL;

	strip_ret(buff);
	pclose(fstream);
	return 0;
}

int get_hardware(char *buff, int len)
{
	int err;

	if (!buff)
		return -EINVAL;
	// 1.check hw version falg
	err = get_file_string("/sys/devices/factory-read/hw_ver_flag",
			    "hexdump -c -n2 -s 27 /dev/mtdblock2", buff, len);
	if (err)
		return err;

	if (strncmp(buff, "hv", 2))
		return -ENODATA;

	// 2.get the version
	return get_file_string("/sys/devices/factory-read/hw_ver",
			    "hexdump -c -n32 -s 29 /dev/mtdblock2", buff, len);
}

int get_version(char *buff, int len)
{
	if (!buff)
		return -EINVAL;

	return get_file_string("/etc/openwrt_version", NULL, buff, len);
}

int get_model(char *buff, int len)
{
	int err = 0;

	if (!buff)
		return -EINVAL;
	err = get_file_string("/sys/devices/factory-read/model_ver_flag",
			    "hexdump -c -n2 -s 63 /dev/mtdblock2", buff, len);
	if (err)
		return err;

	// model_ver_flag != "mv" means there is no model_ver in the flash.
	if (strncmp(buff, "mv", 2))
		return -ENODATA;

	return get_file_string("/sys/devices/factory-read/model_ver",
			    "hexdump -c -n32 -s 65 /dev/mtdblock2", buff, len);
}
