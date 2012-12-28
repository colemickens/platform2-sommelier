#ifndef BENCH_GL_ALL_TESTS_H_
#define BENCH_GL_ALL_TESTS_H_

namespace glbench {

class TestBase;

TestBase* GetSwapTest();
TestBase* GetClearTest();
TestBase* GetFillRateTest();
TestBase* GetYuvToRgbTest();
TestBase* GetReadPixelTest();
TestBase* GetTriangleSetupTest();
TestBase* GetAttributeFetchShaderTest();
TestBase* GetVaryingsAndDdxyShaderTest();
TestBase* GetWindowManagerCompositingTest(bool scissor);
TestBase* GetTextureReuseTest();
TestBase* GetTextureUpdateTest();
TestBase* GetTextureUploadTest();

} // namespace glbench

#endif // BENCH_GL_ALL_TESTS_H_
