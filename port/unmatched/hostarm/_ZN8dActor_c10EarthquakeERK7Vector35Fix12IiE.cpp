/* HOST ARM RETIRED (run catchup1, ACT4b round 2). This copy forwarded the
 * flat ROM name _ZN8dActor_c10EarthquakeERK7Vector3i to the camera with the
 * strength pushed, because the pin's src body dropped it on x86. Main's
 * src/runtime/actor/d_actor.cpp now defines the real member,
 * dActor_c::Earthquake(const Vector3 &, fx32), which calls
 * gCamera->func_0200d8c8(&pos, magnitude) and so forwards position and
 * strength itself; flat callers reach it through the face ledger. The file
 * stays, empty, so the slice row that lists it still names a real TU until
 * that row and hostgen's park of the flat name are dropped. */
