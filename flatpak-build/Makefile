ARCH ?= $(shell flatpak --default-arch)
REPO ?= repo
FB_ARGS ?= "--user"
TMP ?= app
INSTALL_SOURCE ?= "--install-deps-from=flathub"

build: remotes
	flatpak-builder $(INSTALL_SOURCE) $(FB_ARGS) --arch=$(ARCH) --force-clean --require-changes --ccache --repo=$(REPO) --subject="build of org.kde.xdg-portal-test-kde, `date` (`git rev-parse HEAD`)" ${EXPORT_ARGS} $(TMP) org.kde.xdg-portal-test-kde.json

export:
	flatpak build-update-repo $(REPO) ${EXPORT_ARGS} --generate-static-deltas

$(REPO)/config:
	ostree init --mode=archive-z2 --repo=$(REPO)

remotes:
	flatpak --user remote-add $(ARGS) --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

check:
	json-glib-validate *.json

%.clean: %.json
	json-glib-validate $<
	flatpak-builder --force-clean --arch=$(ARCH) --download-only ${EXPORT_ARGS} app $<

clean:
	rm -rf $(TMP) .flatpak-builder

run:
	flatpak-builder --run $(TMP) org.kde.xdg-portal-test-kde.json xdg-portal-test-kde
