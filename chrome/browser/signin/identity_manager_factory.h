// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace identity {
class IdentityManager;
}

class Profile;

// Singleton that owns all IdentityManager instances and associates them with
// Profiles.
class IdentityManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static identity::IdentityManager* GetForProfile(Profile* profile);
  static identity::IdentityManager* GetForProfileIfExists(
      const Profile* profile);

  // Returns an instance of the IdentityManagerFactory singleton.
  static IdentityManagerFactory* GetInstance();

  // Exposes BuildServiceInstanceFor() publicly for usage to unittests,
  // returning an authenticated IdentityManager, useful specially in
  // ChromeOS scenarios.
  static std::unique_ptr<KeyedService>
  BuildAuthenticatedServiceInstanceForTesting(const std::string& gaia_id,
                                              const std::string& email,
                                              const std::string& refresh_token,
                                              content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<IdentityManagerFactory>;

  IdentityManagerFactory();
  ~IdentityManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_MANAGER_FACTORY_H_
