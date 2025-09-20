# ADRO GFX (notes)

- [Graphics](Src/Graphics/Graphics.md)

## Plan

- [ ] Memory
- [x] Triangle
- [ ] Model
- [x] Camera
- [ ] Smooth Camera
- [ ] Batch Rendering
- [ ] Input System
- [ ] Mouse Picking
- [ ] Gui Integration

## Naming Conventions

### Class Names

- Capitalize the first letter of each words.

### Method / Function names

- Names should be verbs and written in camel case.
- Prefixes are sometimes useful:
    - Is/Has/Can, for bool types
    - Set/Get
    - Initialize
    - Compute
- The name of the class should not be duplicated in a method name.

### Variables

- start with lowercase letter, then capitalize each words.
- Add a comment if the meaning of the variable is not meaningful.

### Pointer Variables

- Place the '*' with the variable name rather than with the type.

### Reference Variables

- Put the '&' with the variable name rather than with the type.
- For class overloaded operators and methods returning a reference, put the '&' with the type.

### Enum Type Names and Enum Names

- Enum types should follow Class Name policy.
- Enum names should be declared using all caps and underscores between words.

### Constants

- Constants should be declared using all CAPS and underscores between words.

### Structure Names

- For structure names, follow conventions for class names with the word 'Type'
- Use of classes is preferred to struct. However, if all data is public, a struct may be used.
- The developer may use structs to encapsulate global data (including exceptions)

```cpp
struct AttitudeTypes
{
    static const RealType TEST_ACCURACY;    // value is 1.19209290E-07F
}
```

### C Function Names

- For C functions use C style function names of all lower case letters with '_' as word delimeter.
- Used them only to interface between C++ and C code.
- 