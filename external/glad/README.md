# GLAD Setup Instructions

1. Go to https://glad.dav1d.de/

2. Configure GLAD with these settings:
   - **Language**: C/C++
   - **Specification**: OpenGL
   - **API gl**: Version 3.3
   - **Profile**: Core
   - **Generate loader**: Yes (checked)

3. Click **"Generate"** button

4. Download the generated zip file

5. Extract the files:
   - Copy the `include/glad/` folder to `external/glad/include/glad/`
   - Copy the `include/KHR/` folder to `external/glad/include/KHR/`
   - Copy `src/glad.c` to `external/glad/src/glad.c`

Your directory structure should look like:
```
external/glad/
├── include/
│   ├── glad/
│   │   └── glad.h
│   └── KHR/
│       └── khrplatform.h
├── src/
│   └── glad.c
└── CMakeLists.txt
```

