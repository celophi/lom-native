# PSY-Z patches: licensing

These patches are applied at build time to a pinned
[PSY-Z](https://github.com/Xeeynamo/psyz) checkout. Each patch carries the
license of the PSY-Z file it modifies, as PSY-Z's own `LICENSE` assigns it:

| PSY-Z files modified | License of the patch |
| --- | --- |
| `psyz/src/platform/`, `psyz/src/psyz/` | MPL-2.0 ([LICENSE](LICENSE) in this directory) |
| `decomp/` | MIT, as in PSY-Z's `decomp/LICENSE` |
| `psyz/include/` | PSY-Z publishes these files without a license; see below |
| `external/SDL/` (PSY-Z's SDL submodule) | zlib, as in SDL's `LICENSE.txt` |

A patch modifying an MPL-2.0 file is a Modification under the MPL and is
licensed under it:

> This Source Code Form is subject to the terms of the Mozilla Public License,
> v. 2.0. If a copy of the MPL was not distributed with this file, You can
> obtain one at https://mozilla.org/MPL/2.0/.

Everything else in this repository is MIT ([LICENSE](../../LICENSE)) unless
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) says otherwise.

Three patches change `psyz/include/`, which PSY-Z lists as unlicensed:
`gte-inv-square-root-header-native.patch` and
`gte-matrix-normal-header-native.patch` add function declarations to
`libgte.h`, and `critical-section-result-header.patch` changes one
declaration's return type in `kernel.h`.
