/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace earblaster {

/* Locate a project data file. `relative` is from the data root
 * (e.g. "skin/lcos/lcos.css") or from brand/ (e.g. "brand/lockup-pill.svg").
 * Search order: $EARBLASTER_DATA, the source tree, then the install DATADIR.
 */
std::string find_data_file(const std::string& relative);

}  // namespace earblaster
