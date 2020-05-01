## C++ Style Guide

We use C++ code style similar to the [MAPS.ME project](https://github.com/mapsme/omim/blob/master/docs/CPP_STYLE.md) with some differences:
- Use **CamelCase** for class names and **snake_case** for other entities like methods, variables, etc.
- Use left-to-right order for variables/params: `const std::string & s` (reference to the const string).
- Do not use prefixes like `m_` for member variables.
