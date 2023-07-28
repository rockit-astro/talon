# HACK: Expand the _topdir path to work around "Dest dir longer than base dir is not supported" errors from rpmbuild
RPMBUILD = rpmbuild --define "_topdir %(pwd)/build/../build/../build/../build/" \
        --define "_builddir %{_topdir}" \
        --define "_rpmdir %{_topdir}" \
        --define "_srcrpmdir %{_topdir}" \
        --define "_sourcedir %(pwd)" \
        --undefine=_disable_source_fetch

all: onemetre

onemetre:
	mkdir -p build
	${RPMBUILD} --define "_version $$(date --utc +%Y%m%d%H%M%S)" -ba rockit-talon.spec
	mv build/x86_64/*.rpm .
	rm -rf build

