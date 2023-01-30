package main

import (
	"fmt"

	"github.com/envoyproxy/envoy/contrib/golang/filters/http/source/go/pkg/api"
	"google.golang.org/protobuf/types/known/structpb"
)

type filter struct {
	callbacks api.FilterCallbackHandler
	path      string
	config    *structpb.Struct
}

func (f *filter) sendLocalReply() api.StatusType {
	headers := make(map[string]string)
	echoBody := ""
	v, ok := f.config.AsMap()["echo_body"]
	if ok {
		echoBody = v.(string)
	}

	body := fmt.Sprintf("%s, path: %s\r\n", echoBody, f.path)
	f.callbacks.SendLocalReply(403, body, headers, -1, "test-from-go")
	return api.LocalReply
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	header.Del("x-test-header-1")
	f.path, _ = header.Get(":path")
	header.Set("rsp-header-from-go", "foo-test")
	// For the convenience of testing, it's better to in the config parse phase
	matchPath, ok := f.config.AsMap()["match_path"]
	if ok && f.path == matchPath.(string) {
		return f.sendLocalReply()
	}
	return api.Continue
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	header.Set("Rsp-Header-From-Go", "bar-test")
	return api.Continue
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) OnDestroy(reason api.DestroyReason) {
}

func (f *filter) Callbacks() api.FilterCallbacks {
	return f.callbacks
}
