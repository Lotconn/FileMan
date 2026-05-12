FROM rockylinux/rockylinux:10.1-ubi

RUN dnf install -y --releasever=10.1 --setop=install_weak_deps=False --nodocs \
        https://dl.fedoraproject.org/pub/epel/epel-release-latest-10.noarch.rpm
RUN sed -i -r '/^\[crb\]/,/^\[/ s/(enabled\s*=\s*)0/\11/g' \
        /etc/yum.repos.d/rocky.repo
RUN dnf dsync -y --releasever=10.1 --setop=install_weak_deps=False --nodocs
RUN dnf install -y --releasever=10.1 --setop=install_weak_deps=False --nodocs \
        epel-release ccache cmake diffutils gcc-c++ nasm ninja-build \
        file-devel qt6-qtbase-devel \
        bash-color-prompt bash-completion procps-ng rootfiles vim
RUN dnf --installroot /mnt/rootfs clean all
RUN rm -rf /var/cache/* /var/log/dnf* \
        /var/log/yum.* /var/lib/*dnf*

RUN cp /usr/share/rootfiles/.* /root && \
    echo "cmake -S . -B build --fresh -G'Ninja Multi-Config' -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release --parallel && strip build/Release/*" >> /root/.bash_history

WORKDIR /src
ENTRYPOINT ["/bin/bash", "-l"]