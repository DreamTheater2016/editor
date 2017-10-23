#include <tui.h>

int init_tui(FILE_BUFFER *buffer)
{
	WIN_DESC *win = buffer->win_desc;
	if (!win)
		return -1;

	initscr();

	buffer->lines->lines = LINES;
	buffer->lines->cols = COLS;

	cbreak();

	printw("works");

	/*mvprintw(10, 10, "hello");
	mvprintw(10, 12, "be");
	
	getch();*/

	return EXIT_SUCCESS;
}

void end_tui(WIN_DESC *win)
{
	getch();
	nocbreak();
	endwin();
}

int get_char(void)
{
	int ch = getch();
	if (ch >= 32 && ch <= 126)
		return NORMAL | ch;
	if (ch >= KEY_F(2) && ch <= KEY_F(7))
		return F_KEY | ch;
	if (ch == KEY_HOME)
		return HOME_KEY;
	if (ch == KEY_END)
		return END_KEY;
	if (ch == KEY_BACKSPACE)
		return BACKSPACE_KEY;
	if (ch == KEY_DL)
		return DELETE_KEY;
	if (ch == 9)
		return TAB_KEY;
	if (ch == KEY_DOWN || ch == KEY_UP || ch == KEY_LEFT || ch == KEY_RIGHT)
		return ARROW | ch;
	if (ch == 10 || ch == KEY_ENTER)
		return ENTER_KEY;
	if (ch == 0x1B)
		return ESCAPE_KEY;
}

void print_lines(LINE_TABLE *l_table)
{
	/*for (size_t y = 0; y < l_table->used; ++y)
	{
	        mvprintw(y, 0, l_table->line[y].buf);
		}*/
	size_t abs = 0;
	for (size_t y = 0; y < l_table->used; ++y)
	{
		move(y, 0);
		for (size_t i = 0; i < l_table->line[y].len; ++i)
		{
			if (abs + l_table->span1 == l_table->gap)
				abs += l_table->gap_len;
			addch(*(l_table->span1 + abs));
			++abs;
		}
	}
}