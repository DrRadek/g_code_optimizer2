#pragma once
#include <string>
#include <memory>

#include "SyncInfo.hpp"

#include <iostream>
#include <glm/gtx/quaternion.hpp>

#include <coroutine>
#include <optional>
#include <cassert>
#include <variant>

struct AlgoRequestBase
{
  bool skipCalculation = false;
};

struct AlgoRequestNewPos : public AlgoRequestBase
{
  shaderio::float3 newPosition;
};

struct AlgoRequestNewQuat : public AlgoRequestBase
{
  glm::quat newQuat;
};


struct AlgoRequestMoveDir : public AlgoRequestBase
{
  shaderio::float2 moveDirection;
};

using AlgoRequestAny = std::variant<AlgoRequestNewPos, AlgoRequestNewQuat, AlgoRequestMoveDir>;

struct RendererResult
{
  float     volume;
  glm::quat rotation;
};
struct AlgoResult
{
  float     bestVolume;
  glm::quat bestRotation;
};

struct AlgoTask
{
  struct promise_type
  {
    // recursion
    std::coroutine_handle<promise_type> parent;
    std::coroutine_handle<promise_type> active;

    std::optional<AlgoRequestAny> algo_request;
    RendererResult                renderer_result{};
    AlgoResult                    algo_result{};

    AlgoTask            get_return_object() { return std::coroutine_handle<promise_type>::from_promise(*this); }
    std::suspend_always initial_suspend() { return {}; }
    auto                final_suspend() noexcept
    {
      struct FinalAwaiter
      {
        bool                    await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
          if(auto p = h.promise().parent)
            return p;
          return std::noop_coroutine();
        }
        void await_resume() noexcept {}
      };
      return FinalAwaiter{};
    }
    void return_value(AlgoResult result) { algo_result = result; }
    void unhandled_exception() { std::terminate(); }
  };

  struct Compute
  {
    AlgoRequestAny                      req;
    std::coroutine_handle<promise_type> h;

    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<promise_type> h_)
    {
      h        = h_;
      auto cur = h;
      while(true)
      {
        cur.promise().algo_request = req;
        cur.promise().active       = h;
        if(!cur.promise().parent)
          break;
        cur = cur.promise().parent;
      }
    }
    RendererResult await_resume()
    {
      auto cur = h;
      while(cur.promise().parent)
        cur = cur.promise().parent;
      return cur.promise().renderer_result;
    }
  };

  std::coroutine_handle<promise_type> h;

  bool                    await_ready() { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> parent)
  {
    h.promise().parent = parent;
    return h;
  }
  void await_resume() {}

  AlgoTask(std::coroutine_handle<promise_type> h)
      : h(h)
  {
  }
  //AlgoTask(AlgoTask&&) = default;
  ~AlgoTask()
  {
    if(h)
      h.destroy();
  }

  AlgoTask(AlgoTask&& other) noexcept
      : h(other.h)
  {
    other.h = nullptr;
  }
  AlgoTask& operator=(AlgoTask&& other) noexcept
  {
    if(this != &other)
    {
      if(h)
        h.destroy();
      h       = other.h;
      other.h = nullptr;
    }
    return *this;
  }
};


class Algorithm
{
public:
  AlgoTask run() { return algorithmLogic(); }

protected:
  float     currentVolume = 0;
  glm::quat currentRotation{};

  float     bestVolume = std::numeric_limits<float>::max();
  glm::quat bestRotation{};

  Algorithm() {}

  void storeRequest(RendererResult result)
  {
    currentRotation = result.rotation;
    currentVolume   = result.volume;
  }

public:
  // Request camera to be set using quaternion
  // Wait for resulting volume
  AlgoTask requestVolumeForQuat(glm::quat newQuat, bool skipCalculation = false)
  {
    storeRequest(co_await AlgoTask::Compute{AlgoRequestNewQuat{skipCalculation, newQuat}});
  }

  // Request camera to be set using direction vector
  // Wait for resulting volume
  AlgoTask requestVolumeForPosition(shaderio::float3 newPosition, bool skipCalculation = false)
  {
    storeRequest(co_await AlgoTask::Compute{AlgoRequestNewPos{skipCalculation, newPosition}});
  }

  // Request camera movement
  // Wait for resulting volume
  AlgoTask requestVolumeForMove(shaderio::float2 move, bool skipCalculation = false)
  {
    storeRequest(co_await AlgoTask::Compute{AlgoRequestMoveDir{skipCalculation, move}});
  }

  // Loop
  virtual AlgoTask algorithmLogic() = 0;

  float getCurrentVolume() { return currentVolume; }

  glm::quat getCurrentRotation() { return currentRotation; }
};

class TestAlgorithm : public Algorithm
{
  AlgoTask algorithmLogic() override
  {
    for(int i = 0; i < 100; ++i)
    {
      std::cout << "requesting volume" << "\n";
      co_await requestVolumeForMove({0.1f, 0.1f});

      std::cout << "Volume is:" << currentVolume << "\n";
      if(currentVolume < bestVolume)
      {
        bestVolume   = currentVolume;
        bestRotation = currentRotation;
      }
    }

    // Finish
    co_return AlgoResult(bestVolume, bestRotation);
  };

public:
  TestAlgorithm()
      : Algorithm()
  {
  }
};
