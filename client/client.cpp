#include <gflags/gflags.h>
#include <iostream>

#include <wangle/bootstrap/ClientBootstrap.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/codec/LineBasedFrameDecoder.h>
#include <wangle/codec/StringCodec.h>

using namespace folly;
using namespace wangle;

DEFINE_int32(port, 8080, "echo server port");
DEFINE_string(host, "::1", "echo server address");

typedef Pipeline<folly::IOBufQueue&, std::string> EchoPipeline;

class EchoHandler : public HandlerAdapter<std::string> {
public:
  virtual void read(Context* ctx, std::string msg) override {
    std::cout << "received back: " << msg;
  }

  virtual void readException(Context* ctx, exception_wrapper e) override {
    std::cout << exceptionStr(e) << std::endl;
    close(ctx);
  }

  virtual void readEOF(Context* ctx) override {
    std::cout << "EOF received :(" << std::endl;
    close(ctx);
  }
};

class EchoPipelineFactory : public PipelineFactory<EchoPipeline> {
public:
  EchoPipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> sock) {
    auto pipeline = EchoPipeline::create();
    pipeline->addBack(AsyncSocketHandler(sock));
    pipeline->addBack(EventBaseHandler());
    pipeline->addBack(LineBasedFrameDecoder(8192, false));
    pipeline->addBack(StringCodec());
    pipeline->addBack(EchoHandler());
    pipeline->finalize();
    return pipeline;
  }
};

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  ClientBootstrap<EchoPipeline> client;
  client.group(std::make_shared<wangle::IOThreadPoolExecutor>(1));
  client.pipelineFactory(std::make_shared<EchoPipelineFactory>());
  auto pipeline = client.connect(SocketAddress(FLAGS_host, FLAGS_port)).get();

  try {
    while (true) {
      std::string line;
      std::getline(std::cin, line);
      if (line == "") {
	break;
      }

      pipeline->write(line + "\r\n").get();
      if (line == "bye") {
	pipeline->close();
	break;
      }
    }
  } catch (const std::exception& e) {
    std::cout << exceptionStr(e) << std::endl;
  }
  
  return 0;
}