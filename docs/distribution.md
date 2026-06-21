# macOS Distribution

If macOS shows `"ClamshellCtl" Not Opened`, the app was blocked by Gatekeeper.
This usually means the app is not Developer ID signed and notarized.

The CLI Homebrew formula is the recommended stable install. The menu bar app cask
is a tester build until signing and notarization are complete.

## For Testers

If you built the app yourself or trust the exact release you installed, you can
allow it on your own Mac:

1. Try to open `ClamshellCtl.app` once.
2. Open System Settings.
3. Go to Privacy & Security.
4. In the Security section, choose Open Anyway for ClamshellCtl.
5. Enter your macOS login password.

Apple says this override is available for about an hour after the blocked open
attempt.

For local development only, you can also remove the quarantine attribute:

```sh
xattr -dr com.apple.quarantine /Applications/ClamshellCtl.app
```

Do not ask normal users to do this as the primary install path. It trains people
to bypass macOS security prompts.

## For Public Distribution

The real fix is to ship a Developer ID signed and notarized app.

Required setup:

- Apple Developer Program membership
- Developer ID Application certificate in Keychain
- App-specific password or App Store Connect API key for `notarytool`

Release flow:

```sh
make clean
make app VERSION=0.4.1

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: Your Name (TEAMID)" \
  build/ClamshellCtl.app/Contents/Resources/clamshellctl

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: Your Name (TEAMID)" \
  build/ClamshellCtl.app

codesign --verify --deep --strict --verbose=2 build/ClamshellCtl.app

ditto -c -k --keepParent build/ClamshellCtl.app build/ClamshellCtl-notary.zip

xcrun notarytool submit build/ClamshellCtl-notary.zip \
  --keychain-profile clamshellctl-notary \
  --wait

xcrun stapler staple build/ClamshellCtl.app
xcrun stapler validate build/ClamshellCtl.app
spctl -a -vv --type execute build/ClamshellCtl.app

ditto -c -k --keepParent build/ClamshellCtl.app build/ClamshellCtl-0.4.1.zip
shasum -a 256 build/ClamshellCtl-0.4.1.zip
```

Then upload the final zip to the GitHub release and update the Homebrew cask
`sha256`.

## Why Ad-Hoc Signing Is Not Enough

The current local build uses ad-hoc signing so the bundle has a consistent code
signature structure during development. Ad-hoc signing does not identify a real
developer to Gatekeeper, and it does not replace notarization.

For general users, the app needs both:

- Developer ID signature
- Apple notarization ticket, preferably stapled to the app before zipping

## References

- Apple Developer: Notarizing macOS software before distribution
  https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
- Apple Developer: Signing apps for Gatekeeper with Developer ID
  https://developer.apple.com/developer-id/
- Apple Support: Open a Mac app from an unknown developer
  https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac
