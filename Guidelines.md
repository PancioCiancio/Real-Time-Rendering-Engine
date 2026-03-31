# The #define Guard

```
#ifndef <PROJECT>_<PATH>_<FILE>_H_
#define <PROJECT>_<PATH>_<FILE>_H_
...
#endif // <PROJECT>_<PATH>_<FILE>_H_
```

# Names and Order of Includes

```
#include "foo/server/fooserver.h"

#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "foo/server/bar.h"
#include "third_party/absl/flags/flag.h"
```

# Enumerator Names

Enumerators should be named like constants.

```cpp
enum class UrlTableError 
{
  kOk = 0,
  kOutOfMemory,
  kMalformedInput,
};
```