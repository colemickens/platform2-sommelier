// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/run_loop.h>
#include <base/threading/simple_thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/dpsl/internal/dpsl_global_context_impl.h"
#include "diagnostics/dpsl/internal/dpsl_thread_context_impl.h"
#include "diagnostics/dpsl/public/dpsl_global_context.h"
#include "diagnostics/dpsl/public/dpsl_thread_context.h"

namespace diagnostics {

class DpslThreadContextImplBaseTest : public testing::Test {
 public:
  DpslThreadContextImplBaseTest() = default;

  ~DpslThreadContextImplBaseTest() override {
    DpslThreadContextImpl::CleanThreadCounterForTesting();
    DpslGlobalContextImpl::CleanGlobalCounterForTesting();
  }

  void SetUp() override {
    global_context_ = DpslGlobalContext::Create();
    ASSERT_TRUE(global_context_);
  }

 protected:
  std::unique_ptr<DpslGlobalContext> global_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DpslThreadContextImplBaseTest);
};

using DpslThreadContextImplBaseDeathTest = DpslThreadContextImplBaseTest;

TEST_F(DpslThreadContextImplBaseDeathTest, CreateWithNullptrGlobalContext) {
  ASSERT_DEATH(DpslThreadContext::Create(nullptr), "GlobalContext is nullptr");
}

TEST_F(DpslThreadContextImplBaseDeathTest, CreateAndForget) {
  ASSERT_TRUE(DpslThreadContext::Create(global_context_.get()));

  ASSERT_DEATH(DpslThreadContext::Create(global_context_.get()),
               "Duplicate DpslThreadContext instances");
}

TEST_F(DpslThreadContextImplBaseDeathTest, CreateAndSave) {
  auto thread_context = DpslThreadContext::Create(global_context_.get());
  ASSERT_TRUE(thread_context);

  ASSERT_DEATH(DpslThreadContext::Create(global_context_.get()),
               "Duplicate DpslThreadContext instances");
}

class DpslThreadContextImplMainThreadTest
    : public DpslThreadContextImplBaseTest {
 public:
  DpslThreadContextImplMainThreadTest() = default;

  void SetUp() override {
    DpslThreadContextImplBaseTest::SetUp();

    main_thread_context_ = DpslThreadContext::Create(global_context_.get());
    ASSERT_TRUE(main_thread_context_);
  }

  void QuitEventLoop() {
    EXPECT_TRUE(main_thread_context_->IsEventLoopRunning());
    main_thread_context_->QuitEventLoop();
  }

  void AddToQueueTask(int task_id) { task_id_queue_.push_back(task_id); }

  void GenerateFailure() {
    ADD_FAILURE() << "This function shouldn't be called";
  }

 protected:
  std::unique_ptr<DpslThreadContext> main_thread_context_;

  std::vector<int> task_id_queue_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DpslThreadContextImplMainThreadTest);
};

TEST_F(DpslThreadContextImplMainThreadTest, BelongsToCurrentThread) {
  EXPECT_TRUE(main_thread_context_->BelongsToCurrentThread());
}

TEST_F(DpslThreadContextImplMainThreadTest, PostTask) {
  main_thread_context_->PostTask(
      std::bind(&DpslThreadContextImplMainThreadTest::QuitEventLoop, this));

  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
  main_thread_context_->RunEventLoop();
  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
}

TEST_F(DpslThreadContextImplMainThreadTest, PostDelayedTask) {
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::QuitEventLoop, this),
      100);

  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
  main_thread_context_->RunEventLoop();
  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
}

TEST_F(DpslThreadContextImplMainThreadTest, PostDelayedTaskDifferentDelays) {
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 3),
      200);
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 2),
      100);
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 1),
      0);

  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::QuitEventLoop, this),
      200);

  // Should not be processsed after quit from event loop.
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::GenerateFailure, this),
      200);

  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
  main_thread_context_->RunEventLoop();
  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());

  EXPECT_THAT(task_id_queue_, testing::ElementsAreArray({1, 2, 3}));
}

TEST_F(DpslThreadContextImplMainThreadTest, PostDelayedTaskSameDelays) {
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 1),
      100);
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 2),
      100);
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::AddToQueueTask, this, 3),
      100);

  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::QuitEventLoop, this),
      100);

  // Should not be processsed after quit from event loop.
  main_thread_context_->PostDelayedTask(
      std::bind(&DpslThreadContextImplMainThreadTest::GenerateFailure, this),
      200);

  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());
  main_thread_context_->RunEventLoop();
  EXPECT_FALSE(main_thread_context_->IsEventLoopRunning());

  EXPECT_THAT(task_id_queue_, testing::ElementsAreArray({1, 2, 3}));
}

using DpslThreadContextImplDeathTest = DpslThreadContextImplMainThreadTest;

TEST_F(DpslThreadContextImplDeathTest, PostDelayedTaskInvalidDelay) {
  ASSERT_DEATH(
      main_thread_context_->PostDelayedTask(
          std::bind(&DpslThreadContextImplMainThreadTest::QuitEventLoop, this),
          -5),
      "Delay must be non-negative");
}

TEST_F(DpslThreadContextImplDeathTest, RunLoopTwice) {
  main_thread_context_->PostTask(
      std::function<void()>([thread_context = main_thread_context_.get()]() {
        thread_context->RunEventLoop();
      }));

  ASSERT_DEATH(main_thread_context_->RunEventLoop(),
               "Called from already running message loop");
}

namespace {

class BackgroundThread : public base::SimpleThread {
 public:
  BackgroundThread(const std::string& name,
                   const base::Closure& on_thread_context_ready,
                   DpslGlobalContext* global_context)
      : base::SimpleThread(name),
        on_thread_context_ready_(on_thread_context_ready),
        global_context_(global_context) {
    DCHECK(global_context);
    DCHECK(!on_thread_context_ready.is_null());
  }

  void Run() override {
    thread_context_ = DpslThreadContext::Create(global_context_);
    ASSERT_TRUE(thread_context_);
    on_thread_context_ready_.Run();

    thread_context_->RunEventLoop();

    thread_context_.reset();
  }

  DpslThreadContext* thread_context() { return thread_context_.get(); }

 private:
  base::Closure on_thread_context_ready_;

  DpslGlobalContext* const global_context_;

  std::unique_ptr<DpslThreadContext> thread_context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundThread);
};

}  // namespace

class DpslThreadContextImplMultiThreadTest
    : public DpslThreadContextImplMainThreadTest {
 public:
  DpslThreadContextImplMultiThreadTest() {
    // The default style "fast" does not support multi-threaded tests.
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  ~DpslThreadContextImplMultiThreadTest() override {
    background_thread_->Join();
  }

  void SetUp() override {
    DpslThreadContextImplMainThreadTest::SetUp();
    SetUpBackgroundThread();
  }

  void SetUpBackgroundThread() {
    base::RunLoop run_loop;

    base::Closure on_thread_context_ready = base::Bind(
        [](const base::Closure& main_thread_callback,
           DpslThreadContext* main_thread_context) {
          ASSERT_TRUE(main_thread_context);
          main_thread_context->PostTask(std::function<void()>(
              [main_thread_callback]() { main_thread_callback.Run(); }));
        },
        run_loop.QuitClosure(), main_thread_context_.get());
    background_thread_ = std::make_unique<BackgroundThread>(
        "background", on_thread_context_ready, global_context_.get());
    background_thread_->Start();

    run_loop.Run();

    ASSERT_TRUE(background_thread_context());
  }

  base::Closure CreateBackgroundEventLoopQuitClosure() const {
    return base::Bind(
        [](DpslThreadContext* background_thread_context) {
          background_thread_context->QuitEventLoop();
        },
        background_thread_context());
  }

  DpslThreadContext* background_thread_context() const {
    DCHECK(background_thread_);
    DpslThreadContext* thread_context = background_thread_->thread_context();
    DCHECK(thread_context);
    return thread_context;
  }

  std::function<void()> CreateFunctionForSyncRunInBackground(
      const base::Closure& main_thread_callback,
      const base::Closure& background_callback) {
    return [background_callback, main_thread_callback,
            main_thread_context = main_thread_context_.get()]() {
      if (!background_callback.is_null()) {
        background_callback.Run();
      }
      ASSERT_TRUE(main_thread_context);
      main_thread_context->PostTask(std::function<void()>(
          [main_thread_callback]() { main_thread_callback.Run(); }));
    };
  }

 protected:
  std::unique_ptr<BackgroundThread> background_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DpslThreadContextImplMultiThreadTest);
};

TEST_F(DpslThreadContextImplMultiThreadTest, PostTask) {
  base::RunLoop run_loop;
  background_thread_context()->PostTask(CreateFunctionForSyncRunInBackground(
      run_loop.QuitClosure(), CreateBackgroundEventLoopQuitClosure()));
  run_loop.Run();
}

TEST_F(DpslThreadContextImplMultiThreadTest, PostDelayedTask) {
  base::RunLoop run_loop;
  background_thread_context()->PostDelayedTask(
      CreateFunctionForSyncRunInBackground(
          run_loop.QuitClosure(), CreateBackgroundEventLoopQuitClosure()),
      100);
  run_loop.Run();
}

class DpslThreadContextImplBackgroundEventLoopAutoStopTest
    : public DpslThreadContextImplMultiThreadTest {
 public:
  DpslThreadContextImplBackgroundEventLoopAutoStopTest() = default;

  void TearDown() override {
    DpslThreadContextImplMultiThreadTest::TearDown();
    base::RunLoop run_loop;
    background_thread_context()->PostTask(CreateFunctionForSyncRunInBackground(
        run_loop.QuitClosure(), CreateBackgroundEventLoopQuitClosure()));
    run_loop.Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DpslThreadContextImplBackgroundEventLoopAutoStopTest);
};

TEST_F(DpslThreadContextImplBackgroundEventLoopAutoStopTest,
       BelongsToCurrentThread) {
  EXPECT_FALSE(background_thread_context()->BelongsToCurrentThread());
}

using DpslThreadContextImplBackgroundEventLoopAutoStopDeathTest =
    DpslThreadContextImplBackgroundEventLoopAutoStopTest;

TEST_F(DpslThreadContextImplBackgroundEventLoopAutoStopDeathTest,
       RunEventLoopCrash) {
  ASSERT_DEATH(background_thread_context()->RunEventLoop(),
               "Called from wrong thread");
}

TEST_F(DpslThreadContextImplBackgroundEventLoopAutoStopDeathTest,
       IsEventLoopRunningCrash) {
  ASSERT_DEATH(background_thread_context()->IsEventLoopRunning(),
               "Called from wrong thread");
}

TEST_F(DpslThreadContextImplBackgroundEventLoopAutoStopDeathTest,
       QuitEventLoopCrash) {
  ASSERT_DEATH(background_thread_context()->QuitEventLoop(),
               "Called from wrong thread");
}

TEST_F(DpslThreadContextImplBackgroundEventLoopAutoStopDeathTest,
       DestructorCrash) {
  ASSERT_DEATH(delete background_thread_context(), "Called from wrong thread");
}

}  // namespace diagnostics
