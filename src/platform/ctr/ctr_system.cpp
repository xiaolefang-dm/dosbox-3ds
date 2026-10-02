#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>

#include "ctr_system.h"
#include "ctr_gfx.h"

typedef struct fileliststruct {
    char filename[64];
} fileliststruct;

static fileliststruct *filelist = NULL;
char conf_path[256]             = {0};

void ctr_backlight_enable(bool enable, u32 screen) {
	u8 device_model = 0xFF;
	CFGU_GetSystemModel(&device_model);
	if (device_model != CFG_MODEL_2DS)
	{
		gspLcdInit();
		enable ? GSPLCD_PowerOnBacklight(screen):GSPLCD_PowerOffBacklight(screen);
		gspLcdExit();
	}
}

void ctr_sys_error(bool fatal, const char* error) {
	if (!gspHasGpuRight())
		gfxInitDefault();

	errorConf msg;
	errorInit(&msg, ERROR_TEXT, CFG_LANGUAGE_EN);
	errorText(&msg, error);
	errorDisp(&msg);

	if (fatal)
		exit(0);

	return;
}

void ctr_check_dsp() {
	if (envIsHomebrew())
		return;

	FILE *dsp = fopen("sdmc:/3ds/dspfirm.cdc", "r");
	if (dsp == NULL) {
		fclose(dsp);
		ctr_sys_error(1, "Cannot find DSP firmware!\n\n\"sdmc:/3ds/dspfirm.cdc\"");
	}
	fclose(dsp);
}

void ctr_wait_for_input ()
{
	printf("Press a key to continue...\n");
	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();

		if (kDown) break;

		gspWaitForVBlank();
	}
}

void ctr_restart() {
	Result res;
	u64 titleId;

	if (envIsHomebrew())
	{
		ctr_sys_error(false,"[.3dsx] Restart not supported.");
		return;
	}

	APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &titleId, NULL, NULL, NULL, NULL);

	res = APT_PrepareToDoApplicationJump(0, titleId, 0x1);
	if (R_FAILED(res))
		ctr_sys_error(true,"CIA cant run, cant prepare.");

	res = APT_DoApplicationJump(NULL, 0, NULL);
	if (R_FAILED(res))
		ctr_sys_error(true,"CIA cant run, cant jump.");

	while(1);
}

static int compare_filenames(const void* a, const void* b) {
    const fileliststruct* file_a = (const fileliststruct*)a;
    const fileliststruct* file_b = (const fileliststruct*)b;
    return strcmp(file_a->filename, file_b->filename);
}

static int findConf(const char* confdir) {
    int i = 0;
    DIR* dp = NULL;
    struct dirent* ds;
    dp = opendir(confdir);
    if (dp != NULL) {
        while ((ds = readdir(dp)) != NULL) {
            if ((strstr(ds->d_name, ".conf")) || (strstr(ds->d_name, ".CONF"))) {
                fileliststruct *new_filelist;
                new_filelist = (fileliststruct*)realloc(filelist, (i + 1) * sizeof(fileliststruct));
                if (new_filelist == NULL) {
                    break;
                }
                filelist = new_filelist;
                memset(&filelist[i], 0, sizeof(fileliststruct));
                strcpy(filelist[i].filename, ds->d_name);
                i++;
            }
        }
        closedir(dp);

        qsort(filelist, i, sizeof(fileliststruct), compare_filenames);
    }
    return i;
}

void ctr_conf_select() {
	int listTotal=0;
	const char* scandir = "sdmc:/3ds/dosbox/config";

	listTotal = findConf(scandir);

	if(listTotal < 1)
		return;

	char listing[45]        = {""};
	int timeout             = 400;
	int listMaxDisplay      = 20;
	int listDisplay         = 0;
	int listCurrentPosition = 0;
	int listScrollPosition  = 0;
	u32 kHeldOld            = 0;
	bool redraw;

	while (aptMainLoop())
	{
		gfxDrawSprite(GFX_BOTTOM, GFX_LEFT, (u8*)ctr_bottom_disable_bgr, 240, 320, 0, 0);

		if (timeout > 100)
		{
			char str_timeout[32];
			sprintf(str_timeout, "Timeout: %i",timeout/100);
			gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, str_timeout , 230, 220);
			redraw = true;
			timeout--;
		}
		else
		{
			if (timeout!=-1) break;
		}

		hidScanInput();

		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();

		if (kDown & KEY_START) break;

        if (kDown & KEY_DUP)
		{
			listCurrentPosition--;
			if((listCurrentPosition+listScrollPosition) < listScrollPosition)
			{
				listScrollPosition--;
				if(listScrollPosition < 0) listScrollPosition = 0;
			}
			if(listCurrentPosition < 0) listCurrentPosition = 0;
		}

		if (kDown & KEY_DDOWN)
		{
			listCurrentPosition++;
			if(listCurrentPosition > listTotal - 1) listCurrentPosition = listTotal - 1;
			if(listCurrentPosition > listMaxDisplay)
	        {
		        if((listCurrentPosition+listScrollPosition) < listTotal) listScrollPosition++;
			    listCurrentPosition = listMaxDisplay;
			}
		}

		if (kDown & KEY_DLEFT)
		{
						listCurrentPosition = listCurrentPosition - 5;
			if((listCurrentPosition+listScrollPosition) < listScrollPosition)
			{
				listScrollPosition = listScrollPosition - 5;
				if(listScrollPosition < 0) listScrollPosition = 0;
			}
			if(listCurrentPosition < 0) listCurrentPosition = 0;
		}

		if (kDown & KEY_DRIGHT)
		{
						listCurrentPosition = listCurrentPosition + 5;
			if(listCurrentPosition > listTotal - 5) listCurrentPosition = listTotal - 5;
			if(listCurrentPosition > listMaxDisplay)
	        {
		        if((listCurrentPosition+listScrollPosition) < listTotal)
				{
					listScrollPosition = listScrollPosition + 5;
				} else {
					listScrollPosition = listTotal-(listMaxDisplay+1);
				}
			    listCurrentPosition = listMaxDisplay;
			}
		}

		if (kHeld != kHeldOld || redraw )
		{

			if (kHeld != kHeldOld)
				timeout=-1;

			for(listDisplay = 0; listDisplay < listTotal; listDisplay++)
			{
				if(listDisplay <= 20)
				{
					int len = strlen(filelist[listDisplay+listScrollPosition].filename)-5;
					strncpy(listing, "",45);
					strncpy(listing, filelist[listDisplay+listScrollPosition].filename, len);

					if(listDisplay == listCurrentPosition)
						gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, (char*) "--->" , (230-(10*listDisplay)), 10);

					gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, listing , (230-(10*listDisplay)), 30);
				}
			}

			kHeldOld = kHeld;
			redraw   = false;

			gfxScreenSwapBuffers(GFX_BOTTOM,false);
		}
		gspWaitForVBlank();
	}

	snprintf(conf_path, sizeof(conf_path), "%s/%s",
			scandir, filelist[listCurrentPosition+listScrollPosition].filename);
	free(filelist);

	gfxDrawSprite(GFX_BOTTOM, GFX_LEFT, (u8*)ctr_bottom_load_bgr, 240, 320, 0, 0);
	gfxScreenSwapBuffers(GFX_BOTTOM,false);

	return;
}

#define CTR_LIST_MAX 256
#define CTR_NAME_MAX 96
#define CTR_LIST_ROWS 14

typedef struct {
	char name[CTR_NAME_MAX];
	u8 is_dir;
} ctr_entry_t;

static ctr_entry_t ctr_entries[CTR_LIST_MAX];
static int ctr_entry_count;

static int ctr_entry_cmp(const void *a, const void *b) {
	const ctr_entry_t *ea = (const ctr_entry_t *)a;
	const ctr_entry_t *eb = (const ctr_entry_t *)b;
	if (ea->is_dir != eb->is_dir)
		return (int)eb->is_dir - (int)ea->is_dir;
	return strcasecmp(ea->name, eb->name);
}

static int ctr_is_root(const char *path) {
	return strcmp(path, "sdmc:/") == 0 || strcmp(path, "sdmc:") == 0;
}

static void ctr_path_parent(char *path) {
	char *slash;
	if (ctr_is_root(path))
		return;
	slash = strrchr(path, '/');
	if (!slash)
		return;
	if (slash == path + 5) {
		strcpy(path, "sdmc:/");
		return;
	}
	*slash = 0;
}

static void ctr_path_join(char *dst, int dstlen, const char *dir, const char *name) {
	size_t n = strlen(dir);
	if (n > 0 && dir[n - 1] == '/')
		snprintf(dst, dstlen, "%s%s", dir, name);
	else
		snprintf(dst, dstlen, "%s/%s", dir, name);
}

static void ctr_start_dir(char *path, int pathlen) {
	snprintf(path, pathlen, "sdmc:/");
}

static int ctr_is_launch_file(const char *name) {
	size_t n = strlen(name);
	if (n < 4)
		return 0;
	if (strcasecmp(name + n - 4, ".bat") == 0)
		return 1;
	if (strcasecmp(name + n - 4, ".exe") == 0)
		return 1;
	if (strcasecmp(name + n - 4, ".com") == 0)
		return 1;
	return 0;
}

static void ctr_scan_dir(const char *path) {
	DIR *dp;
	struct dirent *ds;
	ctr_entry_count = 0;
	dp = opendir(path);
	if (!dp)
		return;
	while ((ds = readdir(dp)) != NULL && ctr_entry_count < CTR_LIST_MAX) {
		char full[512];
		struct stat st;
		if (ds->d_name[0] == '.' && (ds->d_name[1] == 0 || (ds->d_name[1] == '.' && ds->d_name[2] == 0)))
			continue;
		if (strlen(ds->d_name) >= CTR_NAME_MAX)
			continue;
		ctr_path_join(full, (int)sizeof(full), path, ds->d_name);
		int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
		if (!is_dir && !ctr_is_launch_file(ds->d_name))
			continue;
		memset(&ctr_entries[ctr_entry_count], 0, sizeof(ctr_entries[ctr_entry_count]));
		snprintf(ctr_entries[ctr_entry_count].name, CTR_NAME_MAX, "%s", ds->d_name);
		ctr_entries[ctr_entry_count].is_dir = is_dir;
		ctr_entry_count++;
	}
	closedir(dp);
	qsort(ctr_entries, ctr_entry_count, sizeof(ctr_entries[0]), ctr_entry_cmp);
}

static int ctr_is_conf(const char *name) {
	size_t n = strlen(name);
	if (n < 5)
		return 0;
	return strcasecmp(name + n - 5, ".conf") == 0;
}

int ctr_pick_launch(char *out, int outlen) {
	char path[512];
	char full[512];
	char line[160];
	int parent;
	int total;
	int cursor = 0;
	int scroll = 0;
	int hold_tick = 0;
	int touch_lock = 0;
	int last_touch = -1;
	u32 kHeldOld = 0;

	if (!out || outlen < 2)
		return CTR_PICK_NONE;
	out[0] = 0;
	ctr_start_dir(path, (int)sizeof(path));
	ctr_scan_dir(path);

	while (aptMainLoop()) {
		int row;
		int idx;
		int shown;
		const char *tail;
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();

		parent = !ctr_is_root(path);
		total = parent + ctr_entry_count;
		if (cursor >= total)
			cursor = total > 0 ? total - 1 : 0;
		if (cursor < 0)
			cursor = 0;
		if (cursor < scroll)
			scroll = cursor;
		if (cursor >= scroll + CTR_LIST_ROWS)
			scroll = cursor - CTR_LIST_ROWS + 1;

		if (kDown & KEY_START)
			return CTR_PICK_NONE;

		if (kDown & KEY_B) {
			ctr_path_parent(path);
			ctr_scan_dir(path);
			cursor = 0;
			scroll = 0;
			last_touch = -1;
			touch_lock = 8;
		}

		if (kDown & KEY_DUP)
			cursor--;
		else if (kDown & KEY_DDOWN)
			cursor++;
		else if ((kHeld & (KEY_DUP | KEY_DDOWN)) && kHeld == kHeldOld) {
			hold_tick++;
			if (hold_tick > 18 && (hold_tick % 4) == 0)
				cursor += (kHeld & KEY_DUP) ? -1 : 1;
		} else
			hold_tick = 0;

		if (kDown & KEY_DLEFT)
			cursor -= CTR_LIST_ROWS;
		if (kDown & KEY_DRIGHT)
			cursor += CTR_LIST_ROWS;

		if (touch_lock > 0)
			touch_lock--;
		else if (kDown & KEY_TOUCH) {
			touchPosition touch;
			hidTouchRead(&touch);
			row = ((int)touch.py - 16) / 12;
			idx = scroll + row;
			if (row >= 0 && row < CTR_LIST_ROWS && idx >= 0 && idx < total) {
				if (idx == cursor && idx == last_touch)
					kDown |= KEY_A;
				else {
					cursor = idx;
					last_touch = idx;
				}
			}
		}

		if (kDown & KEY_Y) {
			snprintf(out, outlen, "%s", path);
			return CTR_PICK_RUN;
		}

		if (cursor >= total)
			cursor = total > 0 ? total - 1 : 0;
		if (cursor < 0)
			cursor = 0;

		if (kDown & KEY_A) {
			if (parent && cursor == 0) {
				ctr_path_parent(path);
				ctr_scan_dir(path);
				cursor = 0;
				scroll = 0;
				last_touch = -1;
				touch_lock = 8;
			} else {
				idx = cursor - parent;
				if (idx >= 0 && idx < ctr_entry_count) {
					ctr_path_join(full, (int)sizeof(full), path, ctr_entries[idx].name);
					if (ctr_entries[idx].is_dir) {
						snprintf(path, sizeof(path), "%s", full);
						ctr_scan_dir(path);
						cursor = 0;
						scroll = 0;
						last_touch = -1;
						touch_lock = 8;
					} else {
						snprintf(out, outlen, "%s", full);
						return ctr_is_conf(ctr_entries[idx].name) ? CTR_PICK_CONF : CTR_PICK_RUN;
					}
				}
			}
		}

		kHeldOld = kHeld;

		gfxDrawSprite(GFX_BOTTOM, GFX_LEFT, (u8*)ctr_bottom_disable_bgr, 240, 320, 0, 0);
		tail = path;
		if (strlen(path) > 34)
			tail = path + strlen(path) - 34;
		snprintf(line, sizeof(line), "%s%s", (tail == path) ? "" : "...", tail);
		gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, line, 228, 4);
		shown = 0;
		for (row = 0; row < CTR_LIST_ROWS; row++) {
			idx = scroll + row;
			if (idx >= total)
				break;
			if (parent && idx == 0)
				snprintf(line, sizeof(line), "%s..", (idx == cursor) ? ">" : " ");
			else {
				ctr_entry_t *ent = &ctr_entries[idx - parent];
				snprintf(line, sizeof(line), "%s%s%s",
					(idx == cursor) ? ">" : " ",
					ent->is_dir ? "/" : " ",
					ent->name);
			}
			if (strlen(line) > 38)
				line[38] = 0;
			gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, line, 210 - row * 12, 4);
			shown++;
		}
		(void)shown;
		gfxDrawText(GFX_BOTTOM, GFX_LEFT, NULL, (char*)"A open  B back  Y folder  START dos", 8, 4);
		gfxScreenSwapBuffers(GFX_BOTTOM, false);
		gspWaitForVBlank();
	}
	return CTR_PICK_NONE;
}