#include "efi.h"
#include "common.h"
#include "file.h"
#include "graphics.h"
#include "shell.h"
#include "gui.h"
#include "linux.h"

#define WIDTH_PER_CH	8
#define HEIGHT_PER_CH	20
#define CURSOR_WIDTH	4
#define CURSOR_HEIGHT	4
#define EXIT_BUTTON_WIDTH	20
#define EXIT_BUTTON_HEIGHT	20
#define TUX_NAME	L"itu"
#define TUX_POS_X	400
#define TUX_POS_Y	300
#define TUX_WIDTH	84
#define TUX_HEIGHT	100
#define MAX_TUX_BUF	(TUX_WIDTH * TUX_HEIGHT * \
			 sizeof(struct EFI_GRAPHICS_OUTPUT_BLT_PIXEL))

struct EFI_GRAPHICS_OUTPUT_BLT_PIXEL cursor_tmp[CURSOR_HEIGHT][CURSOR_WIDTH] =
{
	{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
	{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
	{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
	{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}
};
int cursor_old_x;
int cursor_old_y;
struct FILE rect_exit_button;
struct EFI_GRAPHICS_OUTPUT_BLT_PIXEL tux_img_buf[TUX_HEIGHT][TUX_WIDTH];

void draw_cursor(int x, int y)
{
	int i, j;
	for (i = 0; i < CURSOR_HEIGHT; i++)
		for (j = 0; j < CURSOR_WIDTH; j++)
			if ((i * j) < CURSOR_WIDTH)
				draw_pixel(x + j, y + i, white);
}

void save_cursor_area(int x, int y)
{
	int i, j;
	for (i = 0; i < CURSOR_HEIGHT; i++) {
		for (j = 0; j < CURSOR_WIDTH; j++) {
			if ((i * j) < CURSOR_WIDTH) {
				cursor_tmp[i][j] = get_pixel(x + j, y + i);
				cursor_tmp[i][j].Reserved = 0xff;
			}
		}
	}
}

void load_cursor_area(int x, int y)
{
	int i, j;
	for (i = 0; i < CURSOR_HEIGHT; i++)
		for (j = 0; j < CURSOR_WIDTH; j++)
			if ((i * j) < CURSOR_WIDTH)
				draw_pixel(x + j, y + i, cursor_tmp[i][j]);
}

void put_cursor(int x, int y)
{
	if (cursor_tmp[0][0].Reserved)
		load_cursor_area(cursor_old_x, cursor_old_y);

	save_cursor_area(x, y);

	draw_cursor(x, y);

	cursor_old_x = x;
	cursor_old_y = y;
}

void put_exit_button(void)
{
	unsigned int hr = GOP->Mode->Info->HorizontalResolution;
	unsigned int x;

	rect_exit_button.rect.x = hr - EXIT_BUTTON_WIDTH;
	rect_exit_button.rect.y = 0;
	rect_exit_button.rect.w = EXIT_BUTTON_WIDTH;
	rect_exit_button.rect.h = EXIT_BUTTON_HEIGHT;
	rect_exit_button.is_highlight = FALSE;
	draw_rect(rect_exit_button.rect, white);

	/* EXITボタン内の各ピクセルを走査(バッテンを描く) */
	for (x = 3; x < rect_exit_button.rect.w - 3; x++) {
		draw_pixel(x + rect_exit_button.rect.x, x, white);
		draw_pixel(x + rect_exit_button.rect.x,
			   rect_exit_button.rect.w - x, white);
	}
}

unsigned char update_exit_button(int px, int py, unsigned char is_clicked)
{
	unsigned char is_exit = FALSE;

	if (is_in_rect(px, py, rect_exit_button.rect)) {
		if (!rect_exit_button.is_highlight) {
			draw_rect(rect_exit_button.rect, yellow);
			rect_exit_button.is_highlight = TRUE;
		}
		if (is_clicked)
			is_exit = TRUE;
	} else {
		if (rect_exit_button.is_highlight) {
			draw_rect(rect_exit_button.rect, white);
			rect_exit_button.is_highlight = FALSE;
		}
	}

	return is_exit;
}

void load_tux(void)
{
	unsigned long long status;
	struct EFI_FILE_PROTOCOL *root;
	struct EFI_FILE_PROTOCOL *file;
	unsigned long long buf_size = MAX_TUX_BUF;

	status = SFSP->OpenVolume(SFSP, &root);
	assert(status, L"error: SFSP->OpenVolume");

	status = root->Open(root, &file, TUX_NAME, EFI_FILE_MODE_READ,
			    EFI_FILE_READ_ONLY);
	assert(status, L"error: root->Open");

	status = file->Read(file, &buf_size, (void *)tux_img_buf);
	check_warn_error(status, L"warning:file->Read");

	status = file->Close(file);
	status = root->Close(root);
}

void put_tux(void)
{
	blt2((unsigned char *)tux_img_buf, TUX_POS_X, TUX_POS_Y,
	     TUX_WIDTH, TUX_HEIGHT);
}

int ls_gui(void)
{
	int file_num;
	struct RECT t;
	int idx;

	ST->ConOut->ClearScreen(ST->ConOut);

	file_num = ls();

	t.x = 0;
	t.y = 0;
	t.w = (MAX_FILE_NAME_LEN - 1) * WIDTH_PER_CH;
	t.h = HEIGHT_PER_CH;
	for (idx = 0; idx < file_num; idx++) {
		file_list[idx].rect.x = t.x;
		file_list[idx].rect.y = t.y;
		file_list[idx].rect.w = t.w;
		file_list[idx].rect.h = t.h;
		draw_rect(file_list[idx].rect, white);
		t.x += file_list[idx].rect.w + WIDTH_PER_CH;

		file_list[idx].is_highlight = FALSE;
	}

	return file_num;
}

void cat_gui(unsigned short *file_name)
{
	ST->ConOut->ClearScreen(ST->ConOut);

	cat(file_name);

	while (getc() != SC_ESC);
}

void gui(void)
{
	unsigned long long status;
	struct EFI_SIMPLE_POINTER_STATE s;
	int px = 0, py = 0;
	unsigned long long waitidx;
	int file_num;
	int idx;
	unsigned char prev_lb = FALSE;
	unsigned char prev_rb = FALSE, executed_rb;
	unsigned char is_exit = FALSE;

	load_tux();

	SPP->Reset(SPP, FALSE);
	file_num = ls_gui();
	put_exit_button();
	put_tux();

	while (!is_exit) {
		ST->BootServices->WaitForEvent(1, &(SPP->WaitForInput), &waitidx);
		status = SPP->GetState(SPP, &s);
		if (!status) {
			/* マウスカーソル座標更新 */
			px += s.RelativeMovementX >> 13;
			if (px < 0)
				px = 0;
			else if (GOP->Mode->Info->HorizontalResolution <=
				 (unsigned int)px)
				px = GOP->Mode->Info->HorizontalResolution - 1;
			py += s.RelativeMovementY >> 13;
			if (py < 0)
				py = 0;
			else if (GOP->Mode->Info->VerticalResolution <=
				 (unsigned int)py)
				py = GOP->Mode->Info->VerticalResolution - 1;

			/* マウスカーソル描画 */
			put_cursor(px, py);

			/* 右クリックの実行済フラグをクリア */
			executed_rb = FALSE;

			/* ファイルアイコン処理ループ */
			for (idx = 0; idx < file_num; idx++) {
				if (is_in_rect(px, py, file_list[idx].rect)) {
					if (!file_list[idx].is_highlight) {
						draw_rect(file_list[idx].rect,
							  yellow);
						file_list[idx].is_highlight = TRUE;
					}
					if (prev_lb && !s.LeftButton) {
						if (file_list[idx].name[0] != L'i')
							cat_gui(file_list[idx].name);
						else
							view(file_list[idx].name);
						file_num = ls_gui();
						put_exit_button();
						put_tux();
					}
					if (prev_rb && !s.RightButton) {
						edit(file_list[idx].name);
						file_num = ls_gui();
						put_exit_button();
						put_tux();
						executed_rb = TRUE;
					}
				} else {
					if (file_list[idx].is_highlight) {
						draw_rect(file_list[idx].rect,
							  white);
						file_list[idx].is_highlight =
							FALSE;
					}
				}
			}

			/* ファイル新規作成・編集 */
			if ((prev_rb && !s.RightButton) && !executed_rb) {
				/* アイコン外を右クリックした場合 */
				dialogue_get_filename(file_num);
				edit(file_list[file_num].name);
				ST->ConOut->ClearScreen(ST->ConOut);
				file_num = ls_gui();
				put_exit_button();
				put_tux();
			}

			/* 終了ボタン更新 */
			is_exit = update_exit_button(px, py,
						     prev_lb && !s.LeftButton);

			/* TUXボタン更新 */
			if (prev_lb && !s.LeftButton) {
				if ((TUX_POS_X <= px) &&
				    (px < (TUX_POS_X + TUX_WIDTH)) &&
				    (TUX_POS_Y <= py) &&
				    (py < TUX_POS_Y + TUX_HEIGHT))
					boot_linux();
			}

			/* マウスの左右ボタンの前回の状態を更新 */
			prev_lb = s.LeftButton;
			prev_rb = s.RightButton;
		}
	}
}
