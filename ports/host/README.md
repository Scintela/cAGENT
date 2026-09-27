# Host Port

Host support is currently supplied by test callback fixtures rather than a distributable Port
package. A future package may provide a monotonic clock and a test-only HTTP Transport, but it
must not become a Core dependency or define MCU memory behavior.
