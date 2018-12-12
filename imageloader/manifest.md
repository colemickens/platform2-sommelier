# ImageLoader Manifest Parser

Library for parsing ImageLoader manifest files. ImageLoader uses these files
to verify and mount images (e.g. DLC images).

## Fields

- `image-sha256-hash`: Image hash.
- `table-sha256-hash`: Hash of the `dm-verity` table file.
- `fs-type`: File-system type.
- `manifest-version`: Required for compatibility.
- `version`: e.g. DLC version.
- `is-removable`: True if ImageLoader may unload the image.
- `metadata`: Expandable metadata.
- `image-type`: Type of image, e.g. "component", "dlc", etc. Needed for
verification and mount flow. Currently only required for DLC.
- `pre-allocated-size`: Size to be preallocated for DLC image files.
- `size`: Size of the DLC partition inside the image file.
- `id`: Formal ID for a DLC.
- `name`: Human readable DLC name.
