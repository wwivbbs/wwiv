# Releasing WWIV

How release branches, version numbers and build tags work, and the steps for
starting a new release. The example below starts 5.11 after 5.10; substitute
the real numbers.

## How it fits together

* **main** is the next release. Every push to main builds release binaries and
  publishes a GitHub prerelease.
* **Release branches** (`wwiv58`, `wwiv59`, `wwiv5.10`, ...) hold an older
  release that still gets fixes. Every push to a release branch builds and
  publishes binaries for that release.
* **Build tags** are `<label>_<build number>`, for example `wwiv5.10_3779`.
  The build number is shared by all branches, so it only goes up and never
  repeats. Release branches up to 5.9 used labels without a dot (`wwiv59`);
  from 5.10 on the label has a dot between major and minor (`wwiv5.10`), so a
  two-digit minor version can't be mistaken for an older one (`wwiv510` reads
  like `wwiv51`).
* **Release archives** are named `wwiv-<distro>-<version>.<build>.<zip|tar.gz>`,
  for example `wwiv-win-x86-5.10.0.3779.zip`.

The binaries workflow is `.github/workflows/wwiv-binaries.yml` and the PR
checks are `.github/workflows/pr-build.yml`. For pull requests GitHub reads
the workflow files from the PR's base branch, and for pushes from the branch
pushed to, so each branch's copy only needs to list that branch.

## Starting a new release

### 1. Cut the release branch from main

```
git checkout main
git pull
git push origin main:wwiv5.10
```

### 2. Point CI at the release branch (on the release branch)

Open a PR against the new branch that changes:

* `.github/workflows/wwiv-binaries.yml`: under `on: push: branches:`, replace
  `main` with the release branch.
* `.github/workflows/pr-build.yml`: under `on: pull_request: branches:`,
  replace `main` with the release branch. Leave the `push` trigger as `main`:
  only pushes to main may save the shared compiler and vcpkg caches.

Merging this PR is a push to the release branch, so it builds and publishes
the first binaries from that branch.

### 3. Bump the version on main

Open a PR against main that changes:

* `.github/workflows/wwiv-binaries.yml`:
  * `name:` and `run-name:` (`WWIV 5.11`, `WWIV 5.11 CI`)
  * `WWIV_RELEASE` in the top-level `env:` (`5.11.0`)
  * `WWIV_RELEASE_LABEL` in the top-level `env:` (`wwiv5.11`)
* `cmake/Common.cmake`: `set(WWIV_RELEASE "5.11.0" ...)`
* `vcpkg.json`: `"version-string": "5.11.0"`
* `install/docs/whatsnew.txt`: add an empty `What's New in WWIV 5.11.0`
  section above the previous release's section.

Nothing in the C++ code needs to change. The release version is only used as
a display string (`core::short_version()`, `core::full_version()`); the
binary data version (`wwiv_num_version` in `core/version.cpp`) is separate
and only changes when on-disk formats do.

After merging, check that the first main build publishes a
`wwiv5.11_<build>` prerelease with archives named `...-5.11.0.<build>...`.

### 4. Check anything outside this repo that reads the tags

Anything that picks "the newest build" by sorting tag names as text will
put `wwiv5.10_*` before `wwiv59_*`. Sort by build number (the part after
the `_`) or by release date instead.

## Fixing a released version

Fix on main first, then backport:

1. Merge the fix to main with a normal PR.
2. Cherry-pick it onto a branch off the release branch and open a PR
   against the release branch:

   ```
   git checkout -b fix-foo-5.10 origin/wwiv5.10
   git cherry-pick -x <sha from main>
   ```

   `-x` records the original commit in the message, so it is easy to see
   what has been backported.
3. Add the fix to that release's section of `install/docs/whatsnew.txt` on
   the release branch.

Merging the PR publishes a new prerelease from the release branch.

## Older release branches

`wwiv59` and earlier build with the `WWIV59` (etc.) GitHub environment,
which holds that release's `WWIV_RELEASE` variable. Keep those environments
while the branch still gets fixes. Starting with 5.10, the workflow sets the
version in its own `env:` and does not use an environment.
