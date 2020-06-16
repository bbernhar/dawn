# Variable Rate Shading Extension
The Variable Rate Shading extension (VRS) enables variable-rate shading for Dawn.

## Usage

### Per-draw basis
Sets the pipeline fragment shading rate on a per-draw basis. The fragment rate can be dynamically set on the render pass:

```
pass.SetFragmentShadingRate(...)
pass.Draw(..) // shading rate is specified
```

## Impl status

### D3D12 (Variable Rate Shading)
Supports only per-draw basis.

### Vulkan (VK_KHR_variable_rate_fragment_shading)
Unsupported.

### Metal (Varianble Rate Rasterization)
Unsupported.