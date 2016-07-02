
/*

   commands:
		get logger conf objects : objdump -j __logger -t test_logger  | c++filt

   docs:
		linux kernel dynamic logger
		http://mgalgs.github.io/2013/05/10/hacking-your-ELF-for-fun-and-profit.html
		http://stackoverflow.com/questions/16552710/how-do-you-get-the-start-and-end-addresses-of-a-custom-elf-section-in-c-gcc

   about TLS access:
		http://stackoverflow.com/questions/9909980/how-fast-is-thread-local-variable-access-on-linux
		no penalty variable declared and used in application
		twice slower if declared and used in library

 */

#include "logger.hpp"
#include "user.hpp"


void __attribute__ ((noinline)) f1(void){ LOG_ERROR("test f1 error"); }
void __attribute__ ((noinline)) f2(void){ LOG_WARNING("test f2 warning"); }
void __attribute__ ((noinline)) f3(void){ LOG_INFO("test f3 info"); }
void __attribute__ ((noinline)) f4(void){ LOG_TRACE("test f4 trace"); }

void run_f()
{
	f1();
	f2();
	f3();
	f4();
	f5();
}

LOG_ENABLE_FOR_MODULE

int main(void)
{
	Logger::init();

	run_f();
	std::cerr << "enabling" << std::endl;
	Logger::add("f1", Logger::ERROR);
	Logger::add("f2", Logger::ERROR);
	Logger::add("f2", Logger::TRACE);
	Logger::add("f4", Logger::TRACE);
	run_f();

	std::cerr << "disabling" << std::endl;
	Logger::remove("f1");
	Logger::remove("f2");
	run_f();

	std::cerr << "test for some test-client" << std::endl;
	{
		Logger::State s(true);
		run_f();
	}
	std::cerr << "test-client finished" << std::endl;
	run_f();

	std::cerr << "enable all logging" << std::endl;
	Logger::set_all(Logger::TRACE);
	run_f();
	Logger::set_all(0);

	std::cerr << "enable f5 trace" << std::endl;
	Logger::add("f5", Logger::TRACE);
	run_f();


}

