#include <string>
#include <vector>

#include <windows.h>

class console
{
public:
	console();
	~console();
	void initialise(HWND new_window_handle);
	void input(unsigned key);
	void key_down(unsigned key);
	void left_mouse_button_down(unsigned x, unsigned y);
	void left_mouse_button_up(unsigned x, unsigned y);
	void right_mouse_button_down(unsigned x, unsigned y);
	void mouse_move(unsigned x, unsigned y);
	void mouse_wheel(int direction);
	void draw();
	void resize();

private:
	bool initialised;
	HWND window_handle;
	HWND scrollbar_handle;
	unsigned width;
	unsigned height;

	COLORREF background_colour;
	COLORREF text_colour;

	unsigned border;

	HDC buffer_dc;
	HBITMAP bitmap;
	HFONT font;
	unsigned font_width;
	unsigned font_height;
	HBRUSH background_brush;
	HPEN foreground_pen;

	bool selection;
	unsigned selection_start_x;
	unsigned selection_start_y;

	unsigned selection_x;
	unsigned selection_y;

	std::size_t selection_offset_begin;
	std::size_t selection_offset_end;

	bool scrollbar_click;
	unsigned scrollbar_y;
	int scrollbar_offset;

	std::string content;
	std::string history;
	std::string command;

	std::string command_input_prefix;

	std::size_t command_input_offset;

	bool allow_input;

	unsigned actual_line_count;
	unsigned lines_maximum;
	unsigned letters_per_line_maximum;
	int original_scroll_line_offset;
	int scroll_line_offset;
	std::size_t scroll_string_offset;

	unsigned scrollbar_width;

	unsigned scrollbar_height;

	unsigned scrollbar_inner_width;
	unsigned scrollbar_inner_height;

	unsigned inner_height;

	std::string working_directory;

	std::vector<std::string> directories;
	std::vector<std::string> files;

	bool tabbing;
	std::vector<std::string> tab_strings;
	std::size_t tab_strings_offset;
	std::size_t tab_word_offset;
	std::size_t tab_word_length;

	void create_dc(HDC window_dc);
	void create_font(std::string const & name, unsigned width, unsigned height);
	void free_bitmap_and_dc();
	void draw_text(std::string const & text, unsigned x, unsigned y);
	void draw_partial_line(std::string const & text, unsigned & x, unsigned y);
	void draw_background();
	void draw_content();
	void draw_rectangle(unsigned x, unsigned y, unsigned width, unsigned height);
	void draw_scrollbar();
	void set_text_colour(bool use_selection_colour);

	void process_content();
	void determine_selection(unsigned & selection_first_line, unsigned & selection_last_line, unsigned & selection_line_begin, unsigned & selection_line_end);

	void update();
	void invalidate();

	unsigned ceiling_division(unsigned left, unsigned right);

	void scroll_up();
	void scroll_down();

	void command_input();
	void hit_return();

	void clear_command();

	void parse_command();
	void process_find_data(WIN32_FIND_DATA const & find_data, std::vector<std::string> & directories, std::vector<std::string> & files);
	bool read_directory(std::string const & path, std::vector<std::string> & directories, std::vector<std::string> & files);
	void set_working_directory();

	void process_tab();
};
