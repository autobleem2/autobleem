# docker/repo - the download repository server

What serves `https://autobleem.retromenele.pl`: a Caddy container over the plain directory
`/home/claude/autobleem-repo` on the build server. CLAUDE.md's "The download repository" has the layout of that directory and the tools that write it; this folder is only the server.

    ssh psc-build
    cd ~/autobleem/docker/repo
    mkdir -p ~/autobleem-repo
    docker compose up -d
    docker compose logs -f          # the certificate exchange shows here on the first HTTPS request

`REPO_DIR` and `REPO_HOST` in the environment override the directory and the host name (a LAN copy:
`REPO_HOST=localhost docker compose up -d` and use port 9090). Port 443 and 9090 must be reachable from
the Internet for the certificate to be issued - they were on 2026-09-19.
