#version 310 es

void f() {
  uint r = 4294967295u;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
