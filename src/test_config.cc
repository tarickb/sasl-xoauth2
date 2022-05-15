#include <getopt.h>
#include <sasl/sasl.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "token_store.h"

namespace {

struct Options {
  std::string config_path;
  std::string token_path;
};

bool TryParseCommandLine(int argc, char** argv, Options* out) {
  const char* kShortOptions = "c:r:";
  const option kLongOptions[] = {
    { "config", required_argument, nullptr, 'c' },
    { "token", required_argument, nullptr, 'r' },
    { nullptr, 0, nullptr, 0 } };

  while (true) {
    int opt = getopt_long(argc, argv, kShortOptions, kLongOptions, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'c':
        out->config_path = optarg;
        break;

      case 'r':
        out->token_path = optarg;
        break;

      default:
        return false;
    }
  }

  return true;
}

void PrintUsage(const std::string& base_name) {
  fprintf(stderr,
      "Usage: %s [options]\n\n"
      "Options:\n"
      "  -c, --config=<file>  use <file> for configuration rather than\n"
      "                       system default\n"
      "  -r, --token=<file>   attempt to request a token from the OAuth\n"
      "                       provider using the refresh token in <file>\n",
      base_name.c_str());
}

Options ParseCommandLine(int argc, char** argv) {
  const std::string base_name = basename(argv[0]);
  Options parsed_options;

  if (!TryParseCommandLine(argc, argv, &parsed_options)) {
    PrintUsage(base_name);
    exit(EXIT_FAILURE);
  }

  return parsed_options;
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = ParseCommandLine(argc, argv);

  sasl_xoauth2::Config::EnableLoggingToStderr();
  if (sasl_xoauth2::Config::Init(options.config_path) != SASL_OK) {
    printf("Config check failed.\n");
    return EXIT_FAILURE;
  }
  printf("Config check passed.\n");

  if (!options.token_path.empty()) {
    auto logger =
      sasl_xoauth2::Log::Create(
          sasl_xoauth2::Log::OPTIONS_NONE,
          sasl_xoauth2::Log::TARGET_STDERR);
    auto token_store = sasl_xoauth2::TokenStore::Create(logger.get(),
        options.token_path,
        /*enable_updates=*/false);
    if (token_store->Refresh() != SASL_OK) {
      logger->Flush();
      printf("Token refresh failed.\n");
      return EXIT_FAILURE;
    }
    printf("Token refresh succeeded.\n");
  }

  return EXIT_SUCCESS;
}
