
#ifndef _LOGGER_HPP__
#define _LOGGER_HPP__

#include <string>
#include <iostream>
#include <stdint.h>

namespace Logger {

	extern "C" struct log_state __start___logger[];
	extern "C" struct log_state __stop___logger[];

	enum {
		CRIT,	// always enabled
		ERROR,
		WARNING,
		INFO,
		DEBUG,
		TRACE,
		MAX_LEVEL
	};

	struct log_state {
		const char* func;
		volatile int level;
		log_state(const char* f, int l) : func(f), level(l) {}
	};

	// conditional logging (enable all logging in scope on some condition)
	struct State {
		static __thread bool enabled;
		const bool old;
		State(bool f = false) : old(enabled) {
			enabled = f;
		}
		~State() throw() { enabled = old; }
	};
	//

	void init();

	void add(const std::string& s, int level);
	void remove(const std::string& s);
	void set_all(int level);

	const char* format_time();
	extern const char* levels[MAX_LEVEL];

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

#define LOG_CUSTOM_MESSAGE(_key, _level, _msg) \
	do  { \
		static Logger::log_state __attribute__((section("__logger"))) _marker(_key, 0); \
		if (unlikely(_level <= _marker.level || Logger::State::enabled)) { \
			std::cerr << Logger::format_time() \
			          << '[' << Logger::levels[_level] << ']' << ' ' \
					  << _msg << std::endl; \
		} \
	} while(0)

#define LOG_MESSAGE(level, msg) LOG_CUSTOM_MESSAGE(__func__, level, msg)

#define LOG_CRIT(msg)    LOG_MESSAGE(Logger::CRIT, msg)
#define LOG_ERROR(msg)   LOG_MESSAGE(Logger::ERROR, msg)
#define LOG_WARNING(msg) LOG_MESSAGE(Logger::WARNING, msg)
#define LOG_INFO(msg)    LOG_MESSAGE(Logger::INFO, msg)
#define LOG_DEBUG(msg)   LOG_MESSAGE(Logger::DEBUG, msg)
#define LOG_TRACE(msg)   LOG_MESSAGE(Logger::TRACE, msg)

#define LOG_ENABLE_FOR_MODULE \
extern "C" struct Logger::log_state __start___logger[];\
extern "C" struct Logger::log_state __stop___logger[];\
void create_start_stop(void) \
{ \
 	static int tmp __attribute__((unused)) ;\
	tmp = (uintptr_t)__start___logger | (uintptr_t)__stop___logger;\
}\

} // namespace Logger

#endif /* _LOGGER_HPP__ */


