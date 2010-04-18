#include "console.hpp"

#include <algorithm>

#include <cmath>

#include <nil/string.hpp>
#include <nil/clipboard.hpp>
#include <nil/array.hpp>

console::console():
	initialised(false),
	font(0),

	border(2),

	background_colour(RGB(0, 0, 0)),
	text_colour(RGB(255, 255, 255)),

	scrollbar_width(16),
	scroll_line_offset(0),
	original_scroll_line_offset(0),

	selection(false),
	scrollbar_click(false),

	allow_input(true),
	command_input_offset(0),

	command_input_prefix("> "),

	tabbing(false)
{
	create_font("Lucida Console", 8, 12);

	background_brush = CreateSolidBrush(background_colour);
	foreground_pen = CreatePen(PS_SOLID, 1, text_colour);

	set_working_directory();

	command_input();
}

console::~console()
{
	free_bitmap_and_dc();

	DeleteObject(font);
	DeleteObject(background_brush);
	DeleteObject(foreground_pen);
}

void console::initialise(HWND new_window_handle)
{
	window_handle = new_window_handle;
}

void console::input(unsigned key)
{
	selection = false;
	scrollbar_click = false;

	if(allow_input)
	{
		if(key == VK_TAB)
			process_tab();
		else
		{
			tabbing = false;

			char input = static_cast<char>(key);
			if(input < ' ' || input > '~')
				return;
			command.insert(command_input_offset, 1, input);
			command_input_offset++;
		}
		update();
	}
}

void console::key_down(unsigned key)
{
	selection = false;

	if(key != VK_TAB)
		tabbing = false;

	switch(key)
	{
	case VK_RETURN:
		if(allow_input)
			hit_return();
		break;

	case VK_PRIOR:
		scroll_up();
		break;

	case VK_NEXT:
		scroll_down();
		break;

	case VK_ESCAPE:
		if(allow_input)
			clear_command();
		break;

	case VK_LEFT:
		if(command_input_offset > 0)
			command_input_offset--;
		break;

	case VK_RIGHT:
		command_input_offset = std::min(command_input_offset + 1, command.length());
		break;

	case VK_DELETE:
		if(!command.empty() && command_input_offset != command.length())
			command.erase(command.begin() + command_input_offset);
		break;

	case VK_BACK:
		if(!command.empty() && command_input_offset > 0)
		{
			command_input_offset--;
			command.erase(command.begin() + command_input_offset);
		}
		break;

	case VK_HOME:
		command_input_offset = 0;
		break;

	case VK_END:
		command_input_offset = command.length();
		break;
	}
	update();
}

void console::left_mouse_button_down(unsigned x, unsigned y)
{
	unsigned inner_scrollbar_height = 2 * border + scrollbar_width;
	if(!selection && !scrollbar_click && x <= width - 2 * border - scrollbar_width)
	{
		selection = true;

		selection_start_x = x;
		selection_start_y = y;

		selection_x = x;
		selection_y = y;
	}
	else if(!scrollbar_click && y > inner_scrollbar_height && y < height - inner_scrollbar_height)
	{
		scrollbar_click = true;
		scrollbar_y = y;
		scrollbar_offset = 0;
	}
	else if(y >= border && y <= 2 * border + scrollbar_width)
		scroll_up();
	else if(y >= height - border && y <= height - 2 * border + scrollbar_width)
		scroll_down();

	update();
}

void console::left_mouse_button_up(unsigned x, unsigned y)
{
	if(selection)
	{
		selection = false;
		nil::set_clipboard(content.substr(selection_offset_begin, selection_offset_end - selection_offset_begin));
	}
	else if(scrollbar_click)
	{
		scrollbar_click = false;
		original_scroll_line_offset = scroll_line_offset;
	}

	update();
}

void console::right_mouse_button_down(unsigned x, unsigned y)
{
	//This sucks because we are going to use the RMB for setting the cursor instead
	/*
	if(allow_input)
	{
		std::string clipboard = nil::get_clipboard();
		std::size_t newline_offset = clipboard.find('\n');
		if(newline_offset != std::string::npos)
			clipboard.erase(clipboard.begin() + newline_offset, clipboard.end());
		command += clipboard;
	}
	*/
	if(allow_input)
	{
		unsigned command_lines_length = static_cast<unsigned>(command.length() + command_input_prefix.length());
		unsigned quotient = command_lines_length / letters_per_line_maximum;
		unsigned remainder = command_lines_length % letters_per_line_maximum;
		unsigned command_lines = quotient;
		if(remainder)
			command_lines++;
		if(x > border && x < width - scrollbar_width - 2 * border && y > height - border - command_lines * font_height && y < height - border)
		{
			unsigned offset_x = (x - border) / font_width;
			unsigned offset_y = (y - border - (lines_maximum - command_lines) * font_height) / font_height;
			int offset = offset_x + offset_y * letters_per_line_maximum;
			offset = std::max(offset - static_cast<int>(command_input_prefix.length()), 0);
			offset = std::min(offset, static_cast<int>(command.length()));
			command_input_offset = static_cast<std::size_t>(offset);
			update();
		}
	}
}

void console::mouse_move(unsigned x, unsigned y)
{
	if(selection)
	{
		selection_x = x;
		selection_y = y;

		update();
	}
	else if(scrollbar_click)
	{
		scrollbar_offset = y - scrollbar_y;

		update();
	}
}

void console::mouse_wheel(int direction)
{
	if(direction > 0)
		scroll_up();
	else
		scroll_down();
}

void console::create_dc(HDC window_dc)
{
	RECT client_rectangle;
	GetClientRect(window_handle, &client_rectangle);
	width = static_cast<unsigned>(client_rectangle.right);
	height = static_cast<unsigned>(client_rectangle.bottom);

	buffer_dc = CreateCompatibleDC(window_dc);
	bitmap = CreateCompatibleBitmap(window_dc, width, height);

	SelectObject(buffer_dc, bitmap);
	SelectObject(buffer_dc, font);
	SelectObject(buffer_dc, foreground_pen);

	initialised = true;
}

void console::draw()
{
	PAINTSTRUCT paint_object;
	HDC window_dc = BeginPaint(window_handle, &paint_object);
	if(!initialised)
	{
		create_dc(window_dc);
		process_content();
	}

	draw_background();
	if(width >= 3 * border + font_width + scrollbar_width && height >= 6 * border + font_height + 2 * scrollbar_width)
	{
		draw_content();
		draw_scrollbar();
	}

	BitBlt(window_dc, 0, 0, width, height, buffer_dc, 0, 0, SRCCOPY);

	EndPaint(window_handle, &paint_object);
}

void console::resize()
{
	free_bitmap_and_dc();
	initialised = false;
	draw();
}

void console::create_font(std::string const & name, unsigned width, unsigned height)
{
	font_width = width;
	font_height = height;
	font = CreateFont(height, width, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, name.c_str());
}

void console::free_bitmap_and_dc()
{
	if(initialised)
	{
		DeleteObject(bitmap);
		DeleteDC(buffer_dc);
	}
}

void console::update()
{
	process_content();
	invalidate();
}

void console::invalidate()
{
	InvalidateRect(window_handle, 0, TRUE);
}

void console::draw_text(std::string const & text, unsigned x, unsigned y)
{
	TextOut(buffer_dc, static_cast<int>(x), static_cast<int>(y), text.c_str(), static_cast<int>(text.length()));
}

void console::draw_partial_line(std::string const & text, unsigned & x, unsigned y)
{
	draw_text(text, x, y);
	x += static_cast<unsigned>(text.length()) * font_width;
}

void console::draw_background()
{
	RECT rectangle;
	SetRect(&rectangle, 0, 0, width, height);
	FillRect(buffer_dc, &rectangle, background_brush);
}

void console::draw_content()
{
	std::size_t last_newline_offset = scroll_string_offset;

	if(content.empty())
		return;

	unsigned selection_first_line;
	unsigned selection_last_line;
	unsigned selection_line_begin;
	unsigned selection_line_end;

	determine_selection(selection_first_line, selection_last_line, selection_line_begin, selection_line_end);

	bool first_run = true;
	for(int current_line = lines_maximum - 1; current_line >= 0;)
	{
		std::size_t newline_offset = content.rfind('\n', last_newline_offset - 1);
		std::size_t line_offset;
		if(newline_offset == std::string::npos)
			line_offset = 0;
		else
			line_offset = newline_offset + 1;

		std::string line = content.substr(line_offset, last_newline_offset - line_offset);

		unsigned line_length = static_cast<unsigned>(line.length());
		if(line_length == 0)
			current_line--;
		else
		{
			unsigned line_count = ceiling_division(line_length, letters_per_line_maximum);
			for(; line_count > 0 && current_line >= 0; line_count--, current_line--)
			{
				std::size_t offset = static_cast<std::size_t>((line_count - 1) * letters_per_line_maximum);
				std::size_t length = std::min(static_cast<std::size_t>(letters_per_line_maximum), static_cast<std::size_t>(static_cast<std::size_t>(line_length) - offset));
				std::string substring = line.substr(offset, length);

				unsigned x = border;
				unsigned y = height - border - font_height * (lines_maximum - current_line);

				if(selection)
				{
					unsigned unsigned_current_line = static_cast<unsigned>(current_line);
					unsigned substring_length = static_cast<unsigned>(substring.length());

					unsigned end = std::min(selection_line_end, substring_length);
					unsigned begin = std::min(selection_line_begin, substring_length);

					if(unsigned_current_line == selection_first_line)
					{
						std::string unselected_part = substring.substr(0, selection_line_begin);
						set_text_colour(false);
						draw_partial_line(unselected_part, x, y);

						selection_offset_begin = line_offset + offset + begin;
						if(current_line == selection_last_line)
						{
							unsigned selection_length = end - begin;
							std::string selected_part = substring.substr(begin, selection_length);
							set_text_colour(true);
							draw_partial_line(selected_part, x, y);
							selection_offset_end = selection_offset_begin + selection_length;
							unselected_part = substring.substr(end);
							set_text_colour(false);
							draw_text(unselected_part, x, y);
						}
						else
						{
							std::string selected_part = substring.substr(begin);
							set_text_colour(true);
							draw_text(selected_part, x, y);
						}
					}
					else if(unsigned_current_line > selection_first_line && unsigned_current_line < selection_last_line)
					{
						set_text_colour(true);
						draw_text(substring, x, y);
					}
					else if(unsigned_current_line == selection_last_line)
					{
						std::string selected_part = substring.substr(0, end);
						set_text_colour(true);
						draw_partial_line(selected_part, x, y);
						selection_offset_end = line_offset + offset + end;
						std::string unselected_part = substring.substr(end);
						set_text_colour(false);
						draw_text(unselected_part, x, y);
					}
					else
					{
						set_text_colour(false);
						draw_text(substring, x, y);
					}
				}
				else
				{
					draw_text(substring, x, y);
					if(first_run && allow_input && offset <= command_input_offset)
					{
						first_run = false;
						POINT line_points[2];
						unsigned offset = command_input_offset % letters_per_line_maximum;
						int line_x;
						int line_y = y + font_height + 1 + scroll_line_offset * font_height;
						int difference = letters_per_line_maximum - offset;
						if(difference == 1 || difference == 2)
						{
							line_x = (2 - difference) * font_width + border;
							line_y += font_height;
						}
						else
							line_x = (offset + static_cast<unsigned>(command_input_prefix.length())) * font_width + border;
						line_points[0].x = line_x;
						line_points[0].y = line_y;
						line_points[1].x = line_x + font_width;
						line_points[1].y = line_y;
						Polyline(buffer_dc, line_points, static_cast<int>(nil::countof(line_points)));
					}
				}
			}
		}

		if(line_offset == 0 || newline_offset == 0)
			break;

		last_newline_offset = newline_offset;
	}
}

void console::draw_rectangle(unsigned x, unsigned y, unsigned width, unsigned height)
{
	unsigned x2 = x + width;
	unsigned y2 = y + height;

	POINT rectangle_points[5];
	rectangle_points[0].x = x;
	rectangle_points[0].y = y;
	rectangle_points[1].x = x2;
	rectangle_points[1].y = y;
	rectangle_points[2].x = x2;
	rectangle_points[2].y = y2;
	rectangle_points[3].x = x;
	rectangle_points[3].y = y2;
	rectangle_points[4] = rectangle_points[0];
	Polyline(buffer_dc, rectangle_points, static_cast<int>(nil::countof(rectangle_points)));
}

void console::draw_scrollbar()
{
	unsigned scrollbar_x = width - border - scrollbar_width;
	unsigned scrollbar_y = border;
	draw_rectangle(scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_width);

	scrollbar_y += scrollbar_width + border;

	draw_rectangle(scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height);

	float progress;
	if(actual_line_count > lines_maximum)
		progress = static_cast<float>(original_scroll_line_offset) / static_cast<float>(actual_line_count - lines_maximum);
	else
		progress = 0.0f;
	int inner_y = height - 3 * border - scrollbar_width - inner_height - static_cast<unsigned>(static_cast<float>(scrollbar_inner_height - inner_height) * progress);
	if(scrollbar_click)
		inner_y += scrollbar_offset;
	int vertical_limit = scrollbar_width + 3 * border;
	inner_y = std::max<int>(inner_y, vertical_limit);
	inner_y = std::min<int>(inner_y, height - vertical_limit - inner_height);
	scrollbar_y += border;
	draw_rectangle(scrollbar_x + border, inner_y, scrollbar_inner_width, inner_height);

	scrollbar_y += scrollbar_inner_height + 2 * border;
	draw_rectangle(scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_width);
}

void console::process_content()
{
	content = history + command + " ";

	letters_per_line_maximum = std::max((width - 3 * border - scrollbar_width) / font_width, 1u);
	lines_maximum = std::max((height - 2 * border) / font_height, 1u);
	//lines_maximum = 1;

	std::vector<unsigned> line_lengths;
	std::vector<std::string> lines = nil::string::tokenise(content, "\n");
	for(std::vector<std::string>::iterator i = lines.begin(), end = lines.end(); i != end; i++)
	{
		unsigned current_line_length = static_cast<unsigned>(i->length());
		if(current_line_length)
		{
			unsigned quotient = current_line_length / letters_per_line_maximum;
			unsigned remainder = current_line_length % letters_per_line_maximum;
			for(unsigned i = 0; i < quotient; i++)
				line_lengths.push_back(letters_per_line_maximum);
			if(remainder)
				line_lengths.push_back(remainder);
			(*(line_lengths.end() - 1))++;
		}
		else
			line_lengths.push_back(1);
	}
	/*
	if(!line_lengths.empty())
		(*(line_lengths.end() - 1))--;
		*/

	actual_line_count = static_cast<unsigned>(line_lengths.size());

	scrollbar_height = height - 4 * border - 2 * scrollbar_width;

	scrollbar_inner_width = scrollbar_width - 2 * border;
	scrollbar_inner_height = scrollbar_height - 2 * border;

	inner_height = static_cast<unsigned>(static_cast<float>(std::min(actual_line_count, lines_maximum)) / static_cast<float>(actual_line_count) * static_cast<float>(scrollbar_inner_height));
	inner_height = std::max(inner_height, scrollbar_inner_width);

	if(scrollbar_click)
		scroll_line_offset = original_scroll_line_offset + static_cast<int>(static_cast<float>(-scrollbar_offset) / static_cast<float>(scrollbar_inner_height - inner_height) * (actual_line_count - lines_maximum));
	scroll_line_offset = std::min<int>(scroll_line_offset, actual_line_count - lines_maximum);
	scroll_line_offset = std::max<int>(scroll_line_offset, 0);

	unsigned scroll_offset_counter = 0;
	scroll_string_offset = content.length();
	for(std::vector<unsigned>::reverse_iterator i(line_lengths.end()), end(line_lengths.begin()); i != end && scroll_offset_counter < scroll_line_offset; i++, scroll_offset_counter++)
		scroll_string_offset -= *i;
}

void console::determine_selection(unsigned & selection_first_line, unsigned & selection_last_line, unsigned & selection_line_begin, unsigned & selection_line_end)
{
	if(selection)
	{
		selection_first_line = ceiling_division(selection_start_y - border, font_height) - 1;
		selection_last_line = ceiling_division(selection_y - border, font_height) - 1;

		selection_line_begin = ceiling_division(selection_start_x - border, font_width);
		selection_line_end = ceiling_division(selection_x - border, font_width);

		if(selection_first_line > selection_last_line)
		{
			std::swap(selection_first_line, selection_last_line);
			std::swap(selection_line_begin, selection_line_end);
		}

		if(selection_first_line == selection_last_line && selection_line_begin > selection_line_end)
			std::swap(selection_line_begin, selection_line_end);

		if(selection_line_begin > 0)
			selection_line_begin--;
	}
	else
		set_text_colour(false);
}

void console::set_text_colour(bool use_selection_colour)
{
	SetTextColor(buffer_dc, use_selection_colour ? background_colour : text_colour);
	SetBkColor(buffer_dc, use_selection_colour ? text_colour : background_colour);
}

unsigned console::ceiling_division(unsigned left, unsigned right)
{
	return static_cast<unsigned>(std::ceil(static_cast<float>(left) / static_cast<float>(right)));
}

void console::scroll_up()
{
	scroll_line_offset = std::min<int>(scroll_line_offset + 1, actual_line_count - lines_maximum);
	original_scroll_line_offset = scroll_line_offset;
	update();
}

void console::scroll_down()
{
	if(scroll_line_offset > 0)
		scroll_line_offset--;
	original_scroll_line_offset = scroll_line_offset;
	update();
}

void console::command_input()
{
	history += command_input_prefix;
	update();
}

void console::hit_return()
{
	history += command + "\n";
	parse_command();
	clear_command();
	command_input();
}

void console::clear_command()
{
	command.clear();
	command_input_offset = 0;
}

void console::parse_command()
{
	std::string first_token;
	std::size_t space_offset = command.find(' ');
	if(space_offset == std::string::npos)
		first_token = command;
	else
		first_token = command.substr(0, space_offset);

	if(first_token == "pwd")
		history += working_directory + "\n";
	else if(first_token == "dir")
	{
		std::string target;
		if(command.length() > 4)
			target = command.substr(space_offset + 1);
		else
			target = working_directory;
		bool success = read_directory(target, directories, files);
		if(!success)
		{
			history += "Failed to read directory\n";
			return;
		}

		for(std::vector<std::string>::const_iterator i = directories.begin(), end = directories.end(); i != end; i++)
			history += "[D] " + *i + "\n";

		for(std::vector<std::string>::const_iterator i = files.begin(), end = files.end(); i != end; i++)
			history += *i + "\n";
	}
	else if(first_token == "cd")
	{
		if(command.length() < 4)
		{
			history += "Missing argument\n";
			return;
		}
		std::string directory = command.substr(3);
		BOOL result = SetCurrentDirectory(directory.c_str());
		if(result == 0)
		{
			history += "Failed to change directory\n";
			return;
		}
		set_working_directory();
	}
	else
		history += "No such command\n";
}

void console::process_find_data(WIN32_FIND_DATA const & find_data, std::vector<std::string> & directories, std::vector<std::string> & files)
{
	std::string name(find_data.cFileName);
	if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		directories.push_back(name);
	else
		files.push_back(name);
}

bool console::read_directory(std::string const & path, std::vector<std::string> & directories, std::vector<std::string> & files)
{
	WIN32_FIND_DATA find_data;
	std::string target = path + "\\*";
	HANDLE find_handle = FindFirstFile(target.c_str(), &find_data);
	if(find_handle == INVALID_HANDLE_VALUE)
		return false;
	FindNextFile(find_handle, &find_data);
	directories.clear();
	files.clear();
	while(FindNextFile(find_handle, &find_data))
		process_find_data(find_data, directories, files);
	return true;
}

void console::set_working_directory()
{
	char buffer[1024];
	GetCurrentDirectory(static_cast<DWORD>(nil::countof(buffer)), buffer);
	working_directory = buffer;
	read_directory(working_directory, directories, files);
}

void console::process_tab()
{
	if(allow_input)
	{
		if(!tabbing)
		{
			tab_strings.clear();

			std::size_t last_space;
			std::string target;
			if(command.empty())
			{
				last_space = 0;
				for(std::vector<std::string>::const_iterator i = directories.begin(), end = directories.end(); i != end; i++)
					tab_strings.push_back(*i);
				for(std::vector<std::string>::const_iterator i = files.begin(), end = files.end(); i != end; i++)
					tab_strings.push_back(*i);
			}
			else
			{
				last_space = command.rfind(' ', command_input_offset) + 1;
				if(last_space == std::string::npos)
					last_space = 0;
				target = nil::string::trim(command.substr(last_space, command_input_offset - last_space));
				target = nil::string::to_lower(target);
				std::size_t target_length = target.length();
				for(std::vector<std::string>::const_iterator i = directories.begin(), end = directories.end(); i != end; i++)
				{
					std::string const & directory = *i;
					if(target == nil::string::to_lower(directory.substr(0, target_length)))
						tab_strings.push_back(directory);
				}
				for(std::vector<std::string>::const_iterator i = files.begin(), end = files.end(); i != end; i++)
				{
					std::string const & file = *i;
					if(target == nil::string::to_lower(file.substr(0, target_length)))
						tab_strings.push_back(file);
				}
			}

			if(tab_strings.empty())
			{
				MessageBeep(MB_ICONASTERISK);
				return;
			}

			tabbing = true;
			tab_strings_offset = 0;

			std::string const & current_string = tab_strings[tab_strings_offset];

			tab_word_offset = last_space;
			tab_word_length = current_string.length();

			command.replace(last_space, target.length(), current_string);
			command_input_offset = last_space + tab_word_length;
		}
		else
		{
			tab_strings_offset++;
			std::size_t tab_strings_size = tab_strings.size();
			if(tab_strings_offset >= tab_strings_size)
				tab_strings_offset = 0;

			std::string const & current_string = tab_strings[tab_strings_offset];
			command.replace(tab_word_offset, tab_word_length, current_string);

			tab_word_length = current_string.length();

			command_input_offset = tab_word_offset + tab_word_length;
		}
	}
}
