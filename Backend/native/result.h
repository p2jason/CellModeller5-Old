#pragma once

#include <string>
#include <variant>
#include <tuple>
#include <optional>
#include <stdexcept>

/*
 We need some way of handling errors that might occur. If this was an application (and not a library),
 we could just show a dialog window saying that there was an error and then terminate the program. In
 this case, however, we want to handle errors gracefully and pass them to the program using the library.

 We could have used exceptions, but they can cause bugs and prevent the compiler from doing certain
 optimizations. 
*/

struct ErrorMessage
{
	std::string message;
};

template<typename T>
class Result : public std::variant<ErrorMessage, T>
{
public:
	Result(const T& value) : variant(value) {}
	Result(const ErrorMessage& msg) : variant(msg) {}
};

template<>
class Result<void> : public std::variant<ErrorMessage, std::monostate>
{
public:
	Result() : variant(std::monostate{}) {}
	Result(const ErrorMessage& msg) : variant(msg) {}
};

#define CM_ERROR_MESSAGE(msg) ErrorMessage{ std::string(msg) }
#define CM_IS_RESULT_FAILURE(r) (r.index() == 0)
#define CM_PROPAGATE_ERROR(x) { auto r = (x); if (CM_IS_RESULT_FAILURE(r)) return std::get<0>(r); }
#define CM_THROW_ERROR(r) if (CM_IS_RESULT_FAILURE(r)) { throw std::runtime_error(CM_RESULT_ERROR(r).message); }
#define CM_RESULT_VALUE(r) std::get<1>(r)
#define CM_RESULT_ERROR(r) std::get<0>(r)

#define CM_COMBIME_(x, y) x##y
#define CM_COMBIME(x, y) CM_COMBIME_(x, y)

#define CM_TRY(store, ...)	\
auto CM_COMBIME(result__, __LINE__) = (__VA_ARGS__);	\
CM_PROPAGATE_ERROR(CM_COMBIME(result__, __LINE__));	\
store = CM_RESULT_VALUE(CM_COMBIME(result__, __LINE__));

#define CM_TRY_V(...) CM_TRY(auto& CM_COMBIME(_trystore, __LINE__), __VA_ARGS__)

#define CM_TRY_THROW(store, ...)	\
auto CM_COMBIME(result__, __LINE__) = (__VA_ARGS__);	\
CM_THROW_ERROR(CM_COMBIME(result__, __LINE__));	\
store = CM_RESULT_VALUE(CM_COMBIME(result__, __LINE__));

#define CM_TRY_THROW_V(...) CM_TRY_THROW(auto& CM_COMBIME(_trystore, __LINE__), __VA_ARGS__)