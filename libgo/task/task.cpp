#include "task.h"
#include "../common/config.h"
#include <iostream>
#include <string.h>
#include <string>
#include <algorithm>
#include "../debug/listener.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/ref.h"

namespace co
{

void Task::Run()
{
    std::exception_ptr eptr;

    auto call_fn = [this]() {
        if (Listener::GetTaskListener()) {
            Listener::GetTaskListener()->onStart(this->id_);
        }

        this->fn_();
        this->fn_ = TaskF(); //让协程function对象的析构也在协程中执行

        if (Listener::GetTaskListener()) {
            Listener::GetTaskListener()->onCompleted(this->id_);
        }
    };

    if (CoroutineOptions::getInstance().exception_handle == eCoExHandle::immedaitely_throw) {
        call_fn();
        goto end;
    }

    try {
        call_fn();

    } catch (...) {
        this->fn_ = TaskF();

        eptr = std::current_exception();
        if (Listener::GetTaskListener()) {
            Listener::GetTaskListener()->onException(this->id_, eptr);
        }

        if (eptr) {
            const auto handle = CoroutineOptions::getInstance().exception_handle;
            if (handle == eCoExHandle::delay_rethrow) {
                this->eptr_ = eptr;

            } else /*if (handle == eCoExHandle::debugger_only)*/{
                const auto type = (dbg_exception | dbg_task);
                if (CoroutineOptions::getInstance().debug & type) {
                    try {
                        std::rethrow_exception(eptr);
                    } catch (std::exception& e) {
                        DebugPrint(type, "task(%s) has uncaught exception:%s", TaskDebugInfo(this), e.what());
                    } catch (...) {
                        DebugPrint(type, "task(%s) has uncaught exception.", TaskDebugInfo(this));
                    }
                }
            }
        }
    }

    end:
    if (Listener::GetTaskListener()) {
        Listener::GetTaskListener()->onFinished(this->id_, eptr);
    }

    state_ = TaskState::done;
    Scheduler::getInstance().CoYield();
}

Task::Task(TaskF const& fn, std::size_t stack_size)
    : ctx_(stack_size, [this]{Run();}), fn_(fn)
{
//    DebugPrint(dbg_task, "task(%s) construct. this=%p", DebugInfo(), this);
}

Task::~Task()
{
    assert(!this->prev);
    assert(!this->next);
    assert(!this->check_);
//    DebugPrint(dbg_task, "task(%s) destruct. this=%p", DebugInfo(), this);
}

const char* Task::DebugInfo()
{
    return TaskDebugInfo(this);
}

} //namespace co
