## Contributing to the project

We are happy to see that you are looking to help us improve Flow-IPC!  In order to ensure this goes as
easily and quickly as possible, please follow the guidelines below.

### Basics

As mentioned in Organization, you may wish to contribute to the project.  Of course, in your own setting, it
may be perfectly reasonable to simply get a packaged `*_full.tar.gz` or `*_full.zip` archive, make the changes in this
monolithic, non-SCS-mirrored copy; test them locally.  Eventually, though, you'll want to work with the central
Git-SCS-mirrored project.  This section is about that.  It assumes basic familiarity with Git.

As noted in Organization, Flow-IPC is conveniently packaged in a monolithic meta-project.  "Normally"
working with Git in a given repo is straightforward: You clone with a command, you create branch and switch to
it, you make changes/commits, and lastly you issue a pull request (PR) to merge this into the master
development branch.  If this is accepted and merged, you're done!  (Acceptance, as of this writing, means that
code reviewer(s) formally accept(s) your PR.  In addition an automated CI/CD pipeline shall execute for your
PR, initially and after any update; it this fails, then acceptance is unlikely.)

However, as also noted in Organization, Flow-IPC is a meta-project composed of -- primarily -- a handful
of sub-projects: `flow` and `ipc_*`.  (In fact, as of this writing, the dependency tree between them is
a mere queue: `flow` <= `ipc_core` <= `ipc_transport_structured` <= `ipc_session` <= `ipc_shm` <=
`ipc_shm_arena_lend`.)  The meta-project itself bundles them together for convenience and tracking purposes;
and adds a monolithic set of documentation (for `ipc_*`; `flow` has its own, similarly generated,
documentation) and demos/tests.

This organization somewhat (not too much, we think) complicates the aforementioned Git workflow.  Namely:
this meta-project tracks the sub-projects via the Git **submodule** system.  Have a look at
`./.gitmodules`: you'll notice the handful of sub-projects would be cloned to similarly named
directories off the meta-project root (alongside this very `README.md`): `./flow/`, `./ipc_core`,
`./ipc_session`, etc.

Assume the present `README.md` is at Git repo URL `${URL}/ipc.git`.
To clone the entire thing:
  - `git clone --recurse-submodules ${URL}/ipc.git`

If you've cloned it already, without `--recurse-submodules`, then to get the submodules:
  - `git submodule update --init --recursive`

Either way, `.gitmodules` will be consulted as to where to get them (most likely `${URL}/flow.git`,
`${URL}/ipc_core.git`, etc.) and where to place them (which will always be `./flow`, `./ipc_core`, etc.).

A further tutorial regarding how to work with submodules is beyond our scope here (please read official
Git documentation).  However, ultimately, it will still come down to pull requests (PR) to update (most
importantly) the individual sub-projects' master branches; and at times the meta-project's master branch.
The only real added complexity will come from the way the submodule system tracks the most current commit
of each submodule in the parent repo.  The basic idea is, essentially, simple: The `ipc` repo stores not just
the actual files but also a "pointer" to the state of each submodule (`flow`, `ipc_*`) that would be cloned
if one were to invoke the `git clone --recurse-submodules` command above.  Our rule is that in the master
branch of `ipc`, the "pointer" for each submodule is to some commit to that submodule repo's master branch.
Thus the basic procedure is: merge all the relevant PRs into the submodule repos' (if applicable) master
branches; then lastly issue a PR in `ipc` which (possibly among other changes) updates the submodule
pointer(s) to the latest commit in each guy's master branch.

That said, for now at least, we'll leave it to the official Git documentation.

### Some details

This section is not meant as a detailed/formal manual.  This project follows established conventions
and tools of open-source development, Git+GitHub specifically; so the exact steps to follow should come naturally.
We assume either familiarity with such processes or the willingness to learn them.

So in this section we will point out a few specifics that should help and may or may not otherwise be obvious.

The Flow-IPC product (including Flow dependency) lives in the [Flow-IPC organization](https://github.com/Flow-IPC)
at GitHub.  This is open-source.  There is also a [web site hosted using GitHub pages](https://flow-ipc.github.io)]
which hosts, at least, online copies of generated documentation.

The master branch in each repo is called `main`.  Thus any contribution will involve:
  - A change to the code in 0 or more of the submodule repos (`flow`, `ipc_*`).  (So that's 0 or more pull
    requests (PRs) to each relevant `main` branch.)
  - Possibly a change to the code in the `ipc` repo (usually tests).
  - Possibly a change to the submodule pointer(s) in `ipc` repo: 1 for each submodule repo changed above.
    (So that's potentially 1 PR total -- for the last 2 bullet points -- to the `ipc` repo main branch.)

We have some automated CI/CD pipelines.  Namely `flow`, being special as a self-contained project, has the
pipeline steps in `flow/.github/workflows/main.yml` -- this is Flow's dedicated CI/CD pipeline; and `ipc`,
covering Flow-IPC as an overall monolithic project, similarly has Flow-IPC's CI/CD pipeline steps in
`.github/worksflows/main.yml`.  Therefore:
  - Certain automated build/test/doc-generation runs occur when:
    - creating a PR against `flow` repo;
    - updating that PR;
    - finally merging that PR.
  - Certain automated build/test/doc-generation runs occur when:
    - creating a PR against `ipc` repo;
    - updating that PR;
    - finally merging that PR.
  - There are no individual CI/CD pipelines for the `ipc_*` repos; Flow (a special case) aside we treat Flow-IPC as
    a monolithic whole in that sense.

To contribute a change to the project, first of course you'll need to build it per various instructions above
and test the changes locally.  However before these changes will be officially accepted, certain automated
tests will need to pass in the GitHub project, and the changes will need to pass code review.  Here's roughly
how that works.
  1. You create a PR against a repo.
  2. If that repo is `flow` or `ipc`: automated pipeline runs.
     - It builds and tests the code in many configurations, such as Release, Debug, size-minimizing Release,
       Release with debug info, run-time sanitizers ASAN, UBSAN, TSAN.  The matrix includes all those configurations
       across several versions of clang compiler/linker and several versions of gcc compiler/linker.  All in all
       the build/test job runs across ~40 configuration as of this writing.
     - It generates documentation and makes it available as a downloadable artifact (a tarball to download and peruse).
     - If any of that fails, most likely you'll need to update the PR which will re-run the pipeline.
       If it fails, then the pipeline output should make clear what went wrong.
       Could be a build error; could be a test failure; could be Doxygen doc-generation problem.
       Eventually it needs to pass (usually).
  3. A code reviewer will look over your PR.  You know the drill.  Once it is marked approved by at least 1, then
     it becomes mergeable.
  4. You click Merge Pull Request which should now be no longer grayed out.
  5. If that repo is `flow` or `ipc`: automated pipeline runs again, this time off the code in `main`.
     - The generated documentation is checked back into `main`.
     - The web site host (GitHup Pages) is pinged, so that the generated documentation is reflected on
       the [project web site](https://flow-ipc.github.io).

That's the basic idea.  One aspect we haven't covered which bears more detailed explanation is doc generation.
After all, if you've changed the source, then the resulting generated documentation might change.  On that note:
  1. Before making a PR you may wish to locally generate, and visually check, the documentation -- which may
     have changed due to your source code changes.
     This may or may not be necessary, but for now let's assume it is.  Then:
  2. First, follow the aforementioned `cd $BUILD && make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full`
     procedure in Installation.  You may encounter Doxygen warnings; you should fix your code accordingly.
     Next:
  3. Open the result in a browser: `$BUILD/html_ipc_doc_public/index.html`
     and `$BUILD/html_ipc_doc_full/index.html` (the public and full doc sets respectively, for Flow-IPC); and/or
     `$BUILD/flow/html_flow_doc_public/index.html` and `$BUILD/flow/html_flow_doc_full/index.html` (same thing for Flow).
     Ensure things you've changed look good; rinse/repeat if not.
     - If you have changed something under 1 or more `ipc_*/`, then you'll want to check the Flow-IPC documentation.
       Otherwise you can skip that.
     - If you have changed something under `flow/`, then you'll want to check the Flow documentation.  Otherwise
       you can skip that.

You *need not* and *should not* check-in the resulting documentation.  When source code changes are checked-in to
`main` of `flow`, the Flow documentation will be generated and checked-in using our CI/CD
pipeline under `flow/`.  Identically, if one checks-in to `main` of 1 or more of `ipc_*`, and then "seals the deal"
by checking-in the updated submodule pointer(s) to `main` of `ipc`, the Flow-IPC documentation will be generated
and checked-in using the `ipc/` pipeline.  (Search for `git push` in the two `main.yml` files to see what we mean.)
We have already mentioned this above.

The above steps for *locally* generating the documentation are provided only so you can locally test soure code changes' effects on the resulting docs.  Locally generating and verifying docs, after changing source code, is a good idea.
However it's also possible (and for some people/situations preferable) to skip it.
The CI/CD pipeline will mandatorily generate the docs, when a PR is created or updated, as we explained above.
If you did not locally verify the new docs by generating and perusing them, then you must peruse the
doc tarball artifact (mentioned earlier).  If you *did* verify it locally, then you can skip that step.

You can watch all these pipeline runs under Actions tab in GitHub:
  - `flow` repo will have "Flow pipeline" under Actions tab.  (You can view the workflow `main.yml` file which
    is the source code that controls each given pipeline run.)
  - `ipc` repo will have "Flow-IPC pipeline" under Actions tab.  (Ditto regarding `main.yml` viewing.)

### Reporting an issue

Please report issues via GitHub.

### License

By contributing, you agree that your contributions will be licensed under its Apache License, Version
2.0.  (Some sub-projects may use a difference license.  Please check the given file's license header and/or
the nearest LICENSE file, traveling up the directory tree.  However this meta-project, and as of this
writing the majority of sub-projects in the meta-project, indeed use the aforementioned license.)
