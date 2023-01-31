package test.kotlin.integration

import io.envoyproxy.envoymobile.Standard
import io.envoyproxy.envoymobile.EngineBuilder
import io.envoyproxy.envoymobile.RequestHeadersBuilder
import io.envoyproxy.envoymobile.RequestMethod
import io.envoyproxy.envoymobile.UpstreamHttpProtocol
import io.envoyproxy.envoymobile.engine.JniLibrary
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.assertj.core.api.Assertions.assertThat
import org.assertj.core.api.Assertions.fail
import org.junit.Test

private const val testResponseFilterType = "type.googleapis.com/envoymobile.extensions.filters.http.test_remote_response.TestRemoteResponse"
private const val assertionFilterType = "type.googleapis.com/envoymobile.extensions.filters.http.assertion.Assertion"
private const val requestStringMatch = "match_me"

class SendDataTest {
  init {
    JniLibrary.loadTestLibrary()
  }

  @Test
  fun `successful sending data`() {
    val expectation = CountDownLatch(1)
    val engine = EngineBuilder(Standard())
      .addNativeFilter("envoy.filters.http.assertion", "{'@type': $assertionFilterType, match_config: {http_request_generic_body_match: {patterns: [{string_match: $requestStringMatch}]}}}")
      .addNativeFilter("test_remote_response", "{'@type': $testResponseFilterType}")
      .setOnEngineRunning { }
      .build()

    val client = engine.streamClient()

    val requestHeaders = RequestHeadersBuilder(
      method = RequestMethod.GET,
      scheme = "https",
      authority = "example.com",
      path = "/test"
    )
      .addUpstreamHttpProtocol(UpstreamHttpProtocol.HTTP2)
      .build()

    val body = ByteBuffer.wrap(requestStringMatch.toByteArray(Charsets.UTF_8))

    var responseStatus: Int? = null
    var responseEndStream = false
    client.newStreamPrototype()
      .setOnResponseHeaders { headers, endStream, _ ->
        responseStatus = headers.httpStatus
        responseEndStream = endStream
        expectation.countDown()
      }
      .setOnResponseData { _, endStream, _ ->
        responseEndStream = endStream
      }
      .setOnError { _, _ ->
        fail("Unexpected error")
      }
      .start()
      .sendHeaders(requestHeaders, false)
      .close(body)

    expectation.await(10, TimeUnit.SECONDS)

    engine.terminate()

    assertThat(expectation.count).isEqualTo(0)
    assertThat(responseStatus).isEqualTo(200)
    assertThat(responseEndStream).isTrue()
  }
}
