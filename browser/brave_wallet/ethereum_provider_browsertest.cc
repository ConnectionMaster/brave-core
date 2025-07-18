/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/command_line.h"
#include "brave/browser/brave_wallet/brave_wallet_service_factory.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"
#include "brave/components/brave_wallet/browser/test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

namespace {

std::string CheckForEventScript(std::string_view event_var) {
  return absl::StrFormat(R"(
      new Promise(resolve => {
        const timer = setInterval(function () {
          if (%s) {
            clearInterval(timer);
            resolve(true);
          }
        }, 100);
      });
    )",
                         event_var);
}

}  // namespace

namespace brave_wallet {

class EthereumProviderBrowserTest : public InProcessBrowserTest {
 public:
  EthereumProviderBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~EthereumProviderBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    brave_wallet::SetDefaultEthereumWallet(
        browser()->profile()->GetPrefs(),
        brave_wallet::mojom::DefaultWallet::BraveWallet);
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromDirectory(BraveWalletTestDataFolder());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server()->Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  KeyringService* keyring_service() {
    return BraveWalletServiceFactory::GetServiceForContext(browser()->profile())
        ->keyring_service();
  }

  void RestoreWallet() {
    ASSERT_TRUE(keyring_service()->RestoreWalletSync(
        kMnemonicDripCaution, kTestWalletPassword, false));
  }

  void ReloadAndWaitForLoadStop(Browser* browser) {
    chrome::Reload(browser, WindowOpenDisposition::CURRENT_TAB);
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(EthereumProviderBrowserTest, InactiveTabRequest) {
  RestoreWallet();
  GURL url = https_server()->GetURL("a.com", "/ethereum_provider.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* first_tab_web_contents = web_contents();

  // Add a new tab and switch to it
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Add an asset using the first page web contents
  ASSERT_TRUE(
      ExecJs(first_tab_web_contents,
             "wallet_watchAsset('ERC20', "
             "'0x6B175474E89094C44Da98b954EedeAC495271d0F', 'USDC', 6)"));
  base::RunLoop().RunUntilIdle();

  auto result_first =
      EvalJs(first_tab_web_contents, CheckForEventScript("inactiveTabError"));
  EXPECT_EQ(base::Value(true), result_first.value);
}

// This test is flaky on MacOS CI, but mostly works locally.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ActiveTabRequest DISABLED_ActiveTabRequest
#else
#define MAYBE_ActiveTabRequest ActiveTabRequest
#endif
IN_PROC_BROWSER_TEST_F(EthereumProviderBrowserTest, MAYBE_ActiveTabRequest) {
  RestoreWallet();
  GURL url = https_server()->GetURL("a.com", "/ethereum_provider.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Add a new tab and switch to it
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  // Switch back to first tab
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);

  // Add an asset using the first page web contents
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "wallet_watchAsset('ERC20', "
             "'0x6B175474E89094C44Da98b954EedeAC495271d0F', 'USDC', 6)"));
  base::RunLoop().RunUntilIdle();

  auto result_first =
      EvalJs(web_contents(), CheckForEventScript("!inactiveTabError"));
  EXPECT_EQ(base::Value(true), result_first.value);
}

IN_PROC_BROWSER_TEST_F(EthereumProviderBrowserTest,
                       NoCrashOnShortLivedIframes) {
  RestoreWallet();
  GURL url = https_server()->GetURL("a.com", "/short_lived_iframes.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ReloadAndWaitForLoadStop(browser());
}

}  // namespace brave_wallet
