# glTF Viewer Tutorial Code

This is the code repository for https://gltf-viewer-tutorial.gitlab.io/.  
Please refer to the [Rapport-gltf-viewer.pdf]

# HOW TO

### TLDR

All commands to use from the project's root

```bash
source .vscode/bash-init.sh # once per session
cmake_prepare # To setup the project
cmake_build
cmake_install
dist/gltf-viewer viewer gltf-sample-models/2.0/[MODEL_NAME]/glTF/[MODEL_NAME].gltf

# Or after cmake_prepare directly
view_sponza or view_helmet
```

### My git setup

Commit and push to gitlab via VSCODE

then
`git push togithub` to push to gihub

#### Copy-pasted from https://gltf-viewer-tutorial.gitlab.io/roadmap/

Advices  
Don't forget to run source .vscode/bash-init.sh to have access to utility command.  
Download at least one test scene with get_sponza_sample or get_damaged_helmet_sample, or the whole package with clone_gltf_samples
Compile often (with cmake_build command)  
Run often (with view_sponza or view_helmet commands)  
Try to spot bugs early  
Don't freak out: you should have a black screen until you finish "Loading and Drawing" section  
