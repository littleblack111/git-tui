#include "log.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/menu.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include "diff.hpp"
#include "process.hpp"
#include "scroller.hpp"

using namespace ftxui;
namespace log {

std::string ResolveHead() {
  procxx::process git("git");
  git.add_argument("rev-list");
  git.add_argument("HEAD");
  git.add_argument("-1");
  git.exec();
  std::string line;
  std::getline(git.output(), line);
  return line;
}

struct Commit {
  std::wstring hash;
  std::wstring title;
  std::wstring tree;
  std::vector<std::wstring> authors;
  std::vector<std::wstring> body;
  std::vector<std::wstring> committers;
  std::vector<std::wstring> parents;
};

Commit* GetCommit(std::wstring hash) {
  static std::map<std::wstring, std::unique_ptr<Commit>> g_commit;
  if (g_commit[hash])
    return g_commit[hash].get();

  g_commit[hash] = std::make_unique<Commit>();
  Commit* commit = g_commit[hash].get();
  commit->hash = hash;

  procxx::process git("git");
  git.add_argument("cat-file");
  git.add_argument("commit");
  git.add_argument(to_string(hash));
  git.exec();

  std::string line;
  while (std::getline(git.output(), line)) {
    if (line.find("tree ", 0) == 0) {
      commit->tree = to_wstring(line.substr(5));
      continue;
    }

    if (line.find("parent", 0) == 0) {
      commit->parents.push_back(to_wstring(line.substr(7)));
      continue;
    }

    if (line.find("author", 0) == 0) {
      commit->authors.push_back(to_wstring(line.substr(7)));
      continue;
    }

    if (line.find("committer", 0) == 0) {
      commit->committers.push_back(to_wstring(line.substr(10)));
      continue;
    }

    if (line.empty()) {
      int index = -1;
      while (std::getline(git.output(), line)) {
        ++index;
        if (index == 0)
          commit->title = to_wstring(line);
        if (index >= 2)
          commit->body.push_back(to_wstring(line));
      }
      break;
    }
  }
  return commit;
};

int main(int argc, const char** argv) {
  (void)argc;
  (void)argv;
  std::vector<Commit*> commits;
  std::vector<std::wstring> menu_commit_entries;
  int menu_commit_index = 0;

  std::queue<std::wstring> todo;
  todo.push(to_wstring(ResolveHead()));

  auto refresh_commit_list = [&] {
    while (!todo.empty() && menu_commit_index > (int)commits.size() - 80) {
      std::wstring hash = todo.front();
      todo.pop();

      Commit* commit = GetCommit(hash);
      for (auto& parent : commit->parents)
        todo.push(parent);

      menu_commit_entries.push_back(commit->title);
      commits.push_back(commit);
    }
  };
  refresh_commit_list();
  auto menu_commit = Menu(&menu_commit_entries, &menu_commit_index);

  int hunk_size = 3;
  bool split = true;
  std::vector<diff::File> files;
  std::vector<std::wstring> menu_files_entries;
  int menu_files_index = 0;
  auto refresh_files = [&] {
    Commit* commit = commits[menu_commit_index];
    files.clear();
    menu_files_entries.clear();

    procxx::process git("git");
    git.add_argument("diff");
    git.add_argument("-U" + std::to_string(hunk_size));
    git.add_argument(to_string(commit->hash) + "~..." +
                     to_string(commit->hash));
    git.exec();

    std::string diff(std::istreambuf_iterator<char>(git.output()),
                     std::istreambuf_iterator<char>());
    files = diff::Parse(diff);
    menu_files_entries.push_back(L"description");
    for (const auto& file : files)
      menu_files_entries.push_back(file.right_file);
  };
  refresh_files();
  auto menu_files = Menu(&menu_files_entries, &menu_files_index);
  MenuBase::From(menu_commit)->on_change = [&] {
    refresh_files();
    menu_files_index = 0;
  };

  auto commit_renderer = Renderer([&] {
    if (menu_files_index != 0 && menu_files_index - 1 < (int)files.size())
      return diff::Render(files[menu_files_index - 1], split);

    Commit* commit = commits[menu_commit_index];
    Elements elements;

    elements.push_back(hbox({
        text(L"    title:") | bold | color(Color::Red),
        text(commit->title) | xflex,
    }));

    if (commit->body.size() != 0) {
      elements.push_back(separator());
      for (auto& it : commit->body)
        elements.push_back(text(it));
      elements.push_back(separator());
    }

    for (const auto& committer : commit->committers) {
      elements.push_back(hbox({
          text(L"committer:") | bold | color(Color::Green),
          text(committer) | xflex,
      }));
    }

    for (const auto& author : commit->authors) {
      elements.push_back(hbox({
          text(L"   author:") | bold | color(Color::Green),
          text(author) | xflex,
      }));
    }


    elements.push_back(hbox({
        text(L"     hash:") | bold | color(Color::Green),
        text(commit->hash) | xflex,
    }));

    for (const auto& parent : commit->parents) {
      elements.push_back(hbox({
          text(L"   parent:") | bold | color(Color::Green),
          text(parent) | xflex,
      }));
    }

    elements.push_back(hbox({
        text(L"     tree:") | bold | color(Color::Green),
        text(commit->tree) | xflex,
    }));

    auto content = vbox(std::move(elements));
    return content;
  });

  auto scroller = Scroller(commit_renderer);

  auto increase_hunk = [&] {
    hunk_size++;
    refresh_files();
  };
  auto decrease_hunk = [&] {
    if (hunk_size != 0)
      hunk_size--;
    refresh_files();
  };
  auto split_checkbox = Checkbox("[S]plit", &split);
  auto button_increase_hunk = Button("[+1]", increase_hunk, false);
  auto button_decrease_hunk = Button("[-1]", decrease_hunk, false);

  auto options = Container::Horizontal({
      split_checkbox,
      button_decrease_hunk,
      button_increase_hunk,
  });

  auto option_renderer = Renderer(options, [&] {
    return hbox({
               text(L"[git tui log]"),
               filler(),
               split_checkbox->Render(),
               text(L"   Context:"),
               button_decrease_hunk->Render(),
               text(to_wstring(hunk_size)),
               button_increase_hunk->Render(),
               filler(),
           }) |
           bgcolor(Color::White) | color(Color::Black);
  });

  auto container = Container::Vertical({
      options,
      Container::Horizontal({
          menu_commit,
          menu_files,
          scroller,
      }),
  });

  auto renderer = Renderer(container, [&] {
    return vbox({
        option_renderer->Render(),
        hbox({
            vbox({
                text(L"Commit"),
                separator(),
                menu_commit->Render() | size(WIDTH, EQUAL, 25) | yframe,
            }),
            separator(),
            vbox({
                text(L"Files"),
                separator(),
                menu_files->Render() | size(WIDTH, EQUAL, 25) | yframe,
            }),
            separator(),
            vbox({
                text(L"Content"),
                separator(),
                scroller->Render() | xflex,
            }) | xflex,
        }) | yflex | nothing,
    });
  });

  renderer = CatchEvent(renderer, [&](Event event) {
    refresh_commit_list();

    if (event == Event::Character('s')) {
      split = !split;
      return true;
    }

    if (event == Event::Character('-')) {
      decrease_hunk();
      return true;
    }

    if (event == Event::Character('+')) {
      increase_hunk();
      return true;
    }

    return false;
  });

  menu_commit->TakeFocus();

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(renderer);
  return EXIT_SUCCESS;
}

}  // namespace log
