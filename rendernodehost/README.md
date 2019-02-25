# Chrome OS Render Node Forward Service

Render node forward is a quick solution to enable GPU acceleration for VM guests.
In the long term, this should be part of crosvm and be written in Rust. We put the
host part code in a C library now for development velocity.
