#include <ctime>
#include "debug.hxx"

std::map<long, std::string> thread_prefix_map;

void DebugRegisterPrefix( const std::string& prefix ) {
    thread_prefix_map[SGThread::current()] = prefix;
}

std::string DebugTimeToString(time_t& tt)
{
    char buf[256];

    strncpy(buf, ctime(&tt), 255);
    buf[255] = '\0';
    buf[strlen(buf)-1]='\0';

    return std::string( buf );
}
