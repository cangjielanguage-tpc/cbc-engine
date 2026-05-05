#pragma once

namespace Options {

/* C-style string options array syntax:
 *
 * _optStr[size] = { "key1=val1", "key2=val2", ... }
 *
 */
void ParseAndSetOptions(int size, char const** _optStr);

/* CBCOPT enviroment variable syntax:
 *
 * CBCOPT="key1=val1 key2=val2 ..."
 *
 */
void InitEnvOptions();

} // namespace Options
