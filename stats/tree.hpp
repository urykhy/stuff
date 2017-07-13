#pragma once
// auto generated
namespace Stat {
struct FileLoader{
Time loading;
Count file_count;
Time processing{Time::RPS};
Bool error;
void format(std::ostream& os, const std::string& prefix) {
loading.format(os, prefix+'.'+"loading");
file_count.format(os, prefix+'.'+"file_count");
processing.format(os, prefix+'.'+"processing");
error.format(os, prefix+'.'+"error");
}
};
struct Tarantool{
Time insert{Time::RPS};
Time update{Time::RPS};
Time select{Time::RPS};
void format(std::ostream& os, const std::string& prefix) {
insert.format(os, prefix+'.'+"insert");
update.format(os, prefix+'.'+"update");
select.format(os, prefix+'.'+"select");
}
};
struct Common{
Time start{Time::AGO};
Count version;
Count cpu;
Count memory;
void format(std::ostream& os, const std::string& prefix) {
start.format(os, prefix+'.'+"start");
version.format(os, prefix+'.'+"version");
cpu.format(os, prefix+'.'+"cpu");
memory.format(os, prefix+'.'+"memory");
}
};
struct VisitMatch{
Count hits;
Time refresh{Time::AGO};
Count size;
void format(std::ostream& os, const std::string& prefix) {
hits.format(os, prefix+'.'+"hits");
refresh.format(os, prefix+'.'+"refresh");
size.format(os, prefix+'.'+"size");
}
};
struct VisistsStat{
VisitMatch match;
FileLoader http;
Tarantool tnt;
void format(std::ostream& os, const std::string& prefix) {
match.format(os, prefix+'.'+"match");
http.format(os, prefix+'.'+"http");
tnt.format(os, prefix+'.'+"tnt");
}
};
struct Datasource{
Tarantool tnt;
FileLoader http;
void format(std::ostream& os, const std::string& prefix) {
tnt.format(os, prefix+'.'+"tnt");
http.format(os, prefix+'.'+"http");
}
};
struct Main{
Datasource audience;
Datasource games;
VisistsStat visits;
Datasource actions;
Common common;
void format(std::ostream& os, const std::string& prefix) {
audience.format(os, prefix+'.'+"audience");
games.format(os, prefix+'.'+"games");
visits.format(os, prefix+'.'+"visits");
actions.format(os, prefix+'.'+"actions");
common.format(os, prefix+'.'+"common");
}
};
} // namespace Stat
