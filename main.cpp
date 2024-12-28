#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>

#include "argparse.hpp"
#include <iconv.h>

using Time = double;

void assert_good(std::from_chars_result t, const char *what)
{
  if (t.ec != std::errc()) {
    std::cout << "Invalid " << what << ".\n";
    exit(1);
  }
}

Time parse_time(std::string_view view)
{
  int s0 = view.find(':');
  int s1 = view.find(':', s0 + 1);
  int s2 = view.find(',', s1 + 1);
  const char *p = view.data();
  int hour, minute, second, fraction;
  auto result_H = std::from_chars(p, p + s0, hour);
  auto result_m = std::from_chars(p + s0 + 1, p + s1, minute);
  auto result_s = std::from_chars(p + s1 + 1, p + s2, second);
  auto result_f = std::from_chars(p + s2 + 1, p + view.size(), fraction);
  assert_good(result_H, "hour");
  assert_good(result_m, "minute");
  assert_good(result_s, "second");
  assert_good(result_f, "decimal part of second");
  double scale[] = {0.1f, 0.01f, 0.001f, 0.0001f};
  int fraction_size = view.size() - (s2 + 1);
  if (fraction_size > 3) {
    std::cout << "Too many decimals in time: " << view
              << ", fraction=" << view.substr(s2 + 1) << "\n";
    std::cout << "view.size() = " << view.size() << "\n";
    std::cout << "s2 + 1 = " << s2 + 1 << "\n";
    for (char c : view) {
      std::cout << " char: " << (int)c << "\n";
    }
    exit(1);
  }
  return (hour * 3600 + minute * 60 + second)
    + (fraction * scale[fraction_size - 1]);
}

std::string time_to_ass_str(Time t)
{
  int seconds = (int)t;
  int minutes = seconds / 60;
  seconds %= 60;
  int hours = minutes / 60;
  minutes %= 60;
  int centis = (int)((t - (int)t) * 100.0);
  char buf[32];  // give it enough space to shut up the compiler for impossible
                 // numbers.
  std::snprintf(
    buf, sizeof(buf), "%d:%02d:%02d.%02d", hours, minutes, seconds, centis
  );
  return std::string(buf);
}

struct SRT_Subtitle {
  int num;
  Time start, stop;
  std::string text;
};

struct SRT_File {
  std::vector<SRT_Subtitle> subtitles;
};

struct ASS_Subtitle {
  int style;
  Time start, stop;
  std::string text;
};

struct ASS_File {
  std::vector<ASS_Subtitle> subtitles;
};

struct ASS_Subtitle_Comparator {
  bool operator()(const ASS_Subtitle &l, const ASS_Subtitle &r)
  {
    return l.start < r.start;
  }
};

void get_line(std::istream &in, std::string &dst)
{
  std::getline(in, dst);
  if (!dst.empty()) {
    if (dst.back() == '\r') {
      dst = dst.substr(0, dst.size() - 1);
    }
  }
}

SRT_File parse_srt_file(std::istream &in)
{
  SRT_File srt;
  srt.subtitles.reserve(4096);
  std::string line;
  while (!in.eof()) {
    SRT_Subtitle sub;

    // Parse number
    get_line(in, line);
    if (in.eof() || line.empty()) {
      break;
    }
    std::from_chars(line.data(), line.data() + line.size(), sub.num);

    // Parse time info
    get_line(in, line);
    size_t s0 = line.find(' ');
    size_t s1 = line.find(' ', s0 + 1);
    if (s0 == std::string::npos || s1 == std::string::npos) {
      break;
    }
    sub.start = parse_time(std::string_view(line.data(), s0));
    sub.stop =
      parse_time(std::string_view(line.data() + s1 + 1, line.size() - s1 - 1));

    // Get the lines of actual text.
    do {
      get_line(in, line);
      if (line.empty()) {
        break;
      }
      if (in.eof()) {
        break;
      }
      if (sub.text.empty()) {
        sub.text = std::move(line);
      } else {
        sub.text += "\n";
        sub.text += line;
      }
    } while (true);

    // std::cout << "Parsed subtitle: " << sub.text << "\n";

    srt.subtitles.push_back(std::move(sub));
  }
  return srt;
}

void convert_encoding(SRT_File &srt, const char *from, const char *to)
{
  iconv_t cvt = iconv_open(to, from);
  if (cvt == (iconv_t)-1) {
    if (errno == EINVAL)
      std::printf("Conversion from '%s' to '%s' not available\n", from, to);
    else
      perror("iconv_open");

    exit(1);
    return;
  }
  for (size_t i = 0; i < srt.subtitles.size(); ++i) {
    SRT_Subtitle &sub = srt.subtitles[i];
    char out_buf[4096]{0};
    char *out_buf_ptr = (char *)&out_buf;
    char *in_buf;
    size_t in_buf_size;
    size_t out_buf_size;
    in_buf = const_cast<char *>(sub.text.c_str());
    in_buf_size = sub.text.size();
    out_buf_size = sizeof(out_buf);
    size_t result =
      iconv(cvt, &in_buf, &in_buf_size, &out_buf_ptr, &out_buf_size);
    if (result == (size_t)-1) {
      std::cout << "Encoding conversion failed.\n";
      exit(1);
    }
    sub.text = out_buf;
  }
  iconv_close(cvt);
}

void insert_srt_into_ass(ASS_File &ass, const SRT_File &srt, int style)
{
  for (size_t i = 0; i < srt.subtitles.size(); ++i) {
    const SRT_Subtitle &sub = srt.subtitles[i];
    ass.subtitles.push_back({style, sub.start, sub.stop, sub.text});
  }
}

void time_shift(SRT_File &srt, double shift)
{
  for (size_t i = 0; i < srt.subtitles.size(); ++i) {
    SRT_Subtitle &sub = srt.subtitles[i];
    sub.start += shift;
    sub.stop += shift;
  }
}

void replace_all(
  std::string &s,
  std::string const &to_replace,
  std::string const &replace_with
)
{
  std::string buf;
  std::size_t pos = 0;
  std::size_t prevPos;

  // Reserves rough estimate of final size of string.
  buf.reserve(s.size());

  while (true) {
    prevPos = pos;
    pos = s.find(to_replace, pos);
    if (pos == std::string::npos)
      break;
    buf.append(s, prevPos, pos - prevPos);
    buf += replace_with;
    pos += to_replace.size();
  }

  buf.append(s, prevPos, s.size() - prevPos);
  s.swap(buf);
}

std::string text_to_ass_text(std::string text)
{
  replace_all(text, "\r\n", "\n");
  replace_all(text, "\n", "\\N");
  replace_all(text, "<i>", "{\\i1}");
  replace_all(text, "</i>", "{\\i0}");
  replace_all(text, "<b>", "{\\b0}");
  replace_all(text, "</b>", "{\\b0}");
  return text;
}

void write_ass_file(std::ostream &out, const ASS_File &ass)
{
  // clang-format off
  out << "[Script Info]\r\n";
  out << "ScriptType: v4.00+\r\n";
  out << "Collisions: Normal\r\n";
  out << "PlayDepth: 0\r\n";
  out << "Timer: 100,0000\r\n";
  out << "Video Aspect Ratio: 0\r\n";
  out << "WrapStyle: 0\r\n";
  out << "ScaledBorderAndShadow: no\r\n";
  out << "\r\n";
  out << "[V4+ Styles]\r\n";
  out << "Format: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding\r\n";
  //out << "Style: Default,Arial,16,&H00FFFFFF,&H00FFFFFF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,3,0,2,10,10,10,0\r\n";
  out << "Style: Top,Arial,16,&H00F9FFFF,&H00FFFFFF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,3,0,8,10,10,10,0\r\n";
  //out << "Style: Mid,Arial,16,&H0000FFFF,&H00FFFFFF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,3,0,5,10,10,10,0\r\n";
  out << "Style: Bot,Arial,16,&H00F9FFF9,&H00FFFFFF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,3,0,2,10,10,10,0\r\n";
  out << "\r\n";
  out << "[Events]\r\n";
  out << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n";
  // clang-format on

  std::string styles[2] = {"Bot", "Top"};

  for (size_t i = 0; i < ass.subtitles.size(); ++i) {
    const ASS_Subtitle &sub = ass.subtitles[i];
    out << "Dialogue: 0,";
    out << time_to_ass_str(sub.start) << "," << time_to_ass_str(sub.stop);
    out << "," << styles[sub.style] << ",,0000,0000,0000,,";
    out << text_to_ass_text(sub.text);
    out << "\r\n";
  }
}

struct SRT_Subtitle_Time_Comparator {
  bool operator()(const SRT_Subtitle &sub, double time)
  {
    return sub.start < time;
  }
};

double alignment_distance(const SRT_File &a, const SRT_File &b, double offset_b)
{
  double distance = 0.0;
  for (size_t idx_a = 0; idx_a < a.subtitles.size(); ++idx_a) {
    Time start = a.subtitles[idx_a].start - offset_b;
    Time stop = a.subtitles[idx_a].stop - offset_b;

    const double search_window = 8.0; // seconds.

    const auto it_start = std::lower_bound(
      b.subtitles.begin(),
      b.subtitles.end(),
      start - search_window * 0.5,
      SRT_Subtitle_Time_Comparator()
    );

    auto it = it_start;
    auto it_closest = it;
    while (it != b.subtitles.end()) {
      if (std::abs(it->start - start) < std::abs(it_closest->start - start)) {
        it_closest = it;
      }
      if (it->start > start + search_window * 0.5) {
        break;
      }
      it++;
    }

    if (it_closest != b.subtitles.end()) {
      const SRT_Subtitle &sub_b = *it_closest;
      distance += std::abs(sub_b.start - start);
      distance += std::abs(sub_b.stop - stop);
    }
  }
  return distance;
}

int main(int argc, char **argv)
{
  argparse::ArgumentParser program(argv[0]);

  program.add_argument("-b", "--bottom")
    .help("SRT file for the bottom subtitles file.")
    //.required()
    ;
  program.add_argument("--b-enc", "--bottom-enc")
    .help("Encoding of the bottom SRT file.")
    .default_value("UTF-8");
  program.add_argument("--b-shift", "--bottom-tshift")
    .help("Time shift the bottom subtitles")
    .scan<'f', double>();

  program.add_argument("-t", "--top")
    .help("SRT file for the top subtitles file.")
    //.required()
    ;
  program.add_argument("--t-enc", "--top-enc")
    .help("Encoding of the top SRT file.")
    .default_value("UTF-8");

  program.add_argument("--t-shift", "--top-tshift")
    .help("Time shift the top subtitles")
    .scan<'f', double>();
  program.add_argument("--sync-top-to-bottom", "--sync-tb")
    .help(
      "Time synchronize the [arg-0]th subtitle entry of the top SRT file to "
      "the [arg-1]th subtitle entry of the bottom SRT file."
    )
    .nargs(2)
    .scan<'i', int>();
  program.add_argument("--auto-sync-top-to-bottom", "--auto-sync-tb")
    .help(
      "Automatically time synchronize the top SRT file to the bottom SRT file."
    )
    .flag();

  program.add_argument("--output", "-o")
    .help("The output ASS filename.")
    .required();
  program.add_argument("--o-enc")
    .help("Output encoding")
    .default_value("UTF-8");

  try {
    program.parse_args(argc, argv);
  } catch (std::exception &err) {
    std::cout << "Error: " << err.what() << "\n";
    std::cout << program;
    return 1;
  }

  SRT_File bottom_srt;
  if (program.is_used("--bottom")) {
    std::cout << "Reading bottom SRT file...\n";
    std::string srt_filename = program.get("--bottom");
    std::ifstream in(srt_filename);
    bottom_srt = parse_srt_file(in);
    if (program.get("--b-enc") != program.get("--o-enc")) {
      std::cout << "Converting bottom SRT encoding...\n";
      convert_encoding(
        bottom_srt,
        program.get("--b-enc").c_str(),
        program.get("--o-enc").c_str()
      );
    }

    if (bottom_srt.subtitles.empty()) {
      std::cout << "Bottom subtitle file does not contain any subtitles.\n";
      return 1;
    }
  }

  SRT_File top_srt;
  if (program.is_used("--top")) {
    std::cout << "Reading top SRT file...\n";
    std::string srt_filename = program.get("--top");
    std::ifstream in(srt_filename);
    top_srt = parse_srt_file(in);
    if (program.get("--t-enc") != program.get("--o-enc")) {
      std::cout << "Converting top SRT encoding...\n";
      convert_encoding(
        top_srt, program.get("--t-enc").c_str(), program.get("--o-enc").c_str()
      );
    }

    if (top_srt.subtitles.empty()) {
      std::cout << "Top subtitle file does not contain any subtitles.\n";
      return 1;
    }
  }

  std::cout << "Top subtitle file contains " << bottom_srt.subtitles.size()
            << " subtitles.\n";
  std::cout << "Top subtitle file contains " << top_srt.subtitles.size()
            << " subtitles.\n";

  if (program.is_used("--sync-tb")) {
    auto pair = program.get<std::vector<int>>("--sync-tb");
    if (pair[0] >= (int)bottom_srt.subtitles.size() || pair[0] < 0) {
      std::cout << "Subtitle index " << pair[0]
                << " is out of bounds for the bottom subtitle file, which has "
                << bottom_srt.subtitles.size() << " subtitles.\n";
      return 1;
    }
    if (pair[1] >= (int)bottom_srt.subtitles.size() || pair[1] < 0) {
      std::cout << "Subtitle index " << pair[1]
                << " is out of bounds for the top subtitle file, which has "
                << top_srt.subtitles.size() << " subtitles.\n";
      return 1;
    }
    SRT_Subtitle &bot = bottom_srt.subtitles[pair[0]];
    SRT_Subtitle &top = top_srt.subtitles[pair[1]];
    std::cout << "Syncing top to bottom: top[" << pair[1] << "] -> bottom["
              << pair[0] << "]\n";
    std::cout << "  Top   : " << top.text << "\n";
    std::cout << "  Bottom: " << bot.text << "\n";
    double shift = bot.start - top.start;
    std::cout << "Shift: " << shift << "s\n";
    time_shift(top_srt, shift);
  }

  if (program.is_used("--t-shift")) {
    std::cout << "Time shifting top subtitles by: "
              << program.get<double>("--t-shift") << " seconds...\n";
    time_shift(top_srt, program.get<double>("--t-shift"));
  }
  if (program.is_used("--b-shift")) {
    std::cout << "Time shifting bottom subtitles by: "
              << program.get<double>("--b-shift") << " seconds...\n";
    time_shift(bottom_srt, program.get<double>("--b-shift"));
  }

  if (program.is_used("--auto-sync-tb")) {
    std::cout << "Auto syncing...\n";
    double best_distance = std::numeric_limits<double>::max();
    double best_shift = 0;
    for (double shift = -10.0; shift <= 10.001; shift += 0.05) {
      double distance_A = alignment_distance(bottom_srt, top_srt, shift);
      double distance_B = alignment_distance(top_srt, bottom_srt, -shift);
      std::printf("  Attempting shift %+6.2f seconds... Distance: %8.1f | %8.1f\n", shift, distance_A, distance_B);
      if (distance_A + distance_B < best_distance) {
        best_distance = distance_A + distance_B;
        best_shift = shift;
      }
    }
    std::printf("Best shift found: %.2f seconds\n", best_shift);

    time_shift(top_srt, best_shift);
  }

  // Merge them
  ASS_File ass;
  ass.subtitles.reserve(bottom_srt.subtitles.size() + top_srt.subtitles.size());
  insert_srt_into_ass(ass, bottom_srt, 0);
  insert_srt_into_ass(ass, top_srt, 1);
  std::sort(
    ass.subtitles.begin(), ass.subtitles.end(), ASS_Subtitle_Comparator()
  );

  // Write out
  {
    std::ofstream out(program.get("--output"));
    write_ass_file(out, ass);
  }
  return 0;
}
