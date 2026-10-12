package com.qwatch.qlink

import com.qwatch.qlink.irdb.IrParsedButton
import com.qwatch.qlink.irdb.IrParsedRemote
import com.qwatch.qlink.irdb.IrTransmitFeedback
import com.qwatch.qlink.irdb.IrTransmitState
import com.qwatch.qlink.irdb.IrdbParser
import org.junit.Assert.*
import org.junit.Test

class IrdbTransmissionTest {

    @Test
    fun testProtocolSpecificBitLengthsPreservation() {
        val sircIrContent = """
            Filetype: IR library file
            Version: 1
            #
            name: Power
            type: parsed
            protocol: SIRC
            address: 01 00 00 00
            command: 15 00 00 00
            nbits: 12
        """.trimIndent()

        val remote = IrdbParser.parseIrFile(sircIrContent, "sony.ir")
        assertEquals(1, remote.buttons.size)
        val btn = remote.buttons[0]
        assertEquals("Power", btn.name)
        assertEquals("SIRC", btn.protocol)
        assertEquals(12, btn.nbits)
        assertEquals(12, btn.effectiveBits)
        assertEquals(1L, IrParsedButton.parseHexOrFlipperBytes(btn.address))
        assertEquals(21L, IrParsedButton.parseHexOrFlipperBytes(btn.command))
        assertTrue(btn.isProtocolBased)
        assertFalse(btn.isRaw)

        val panasonicContent = """
            Filetype: IR library file
            Version: 1
            name: Vol_Up
            type: parsed
            protocol: Panasonic
            address: 02 20 00 00
            command: 01 00 00 00
            nbits: 48
        """.trimIndent()
        val panaRemote = IrdbParser.parseIrFile(panasonicContent, "panasonic.ir")
        assertEquals(48, panaRemote.buttons[0].nbits)
        assertEquals(48, panaRemote.buttons[0].effectiveBits)

        val nikaiContent = """
            Filetype: IR library file
            Version: 1
            name: Mute
            type: parsed
            protocol: Nikai
            address: 00 00 00 00
            command: 7F 80 00 00
            nbits: 24
        """.trimIndent()
        val nikaiRemote = IrdbParser.parseIrFile(nikaiContent, "nikai.ir")
        assertEquals(24, nikaiRemote.buttons[0].nbits)
        assertEquals(24, nikaiRemote.buttons[0].effectiveBits)
    }

    @Test
    fun testDefaultBitLengthsWhenMissingOrZero() {
        assertEquals(32, IrParsedButton.getDefaultBitsForProtocol("NEC"))
        assertEquals(32, IrParsedButton.getDefaultBitsForProtocol("NECext"))
        assertEquals(42, IrParsedButton.getDefaultBitsForProtocol("NEC42"))
        assertEquals(32, IrParsedButton.getDefaultBitsForProtocol("Samsung32"))
        assertEquals(12, IrParsedButton.getDefaultBitsForProtocol("Sony"))
        assertEquals(12, IrParsedButton.getDefaultBitsForProtocol("SIRC"))
        assertEquals(15, IrParsedButton.getDefaultBitsForProtocol("SIRC15"))
        assertEquals(20, IrParsedButton.getDefaultBitsForProtocol("SIRC20"))
        assertEquals(24, IrParsedButton.getDefaultBitsForProtocol("RCA"))
        assertEquals(24, IrParsedButton.getDefaultBitsForProtocol("Nikai"))
        assertEquals(48, IrParsedButton.getDefaultBitsForProtocol("Panasonic"))
        assertEquals(12, IrParsedButton.getDefaultBitsForProtocol("RC5"))
        assertEquals(13, IrParsedButton.getDefaultBitsForProtocol("RC5X"))
        assertEquals(20, IrParsedButton.getDefaultBitsForProtocol("RC6"))

        // When nbits is omitted (0), effectiveBits falls back to protocol-specific default
        val btnSony = IrParsedButton(name = "Test", protocol = "SIRC", nbits = 0)
        assertEquals(12, btnSony.effectiveBits)

        val btnRca = IrParsedButton(name = "Test", protocol = "RCA", nbits = 0)
        assertEquals(24, btnRca.effectiveBits)

        val btnPanasonic = IrParsedButton(name = "Test", protocol = "Panasonic", nbits = 0)
        assertEquals(48, btnPanasonic.effectiveBits)

        // When unknown protocol and nbits is 0, fallback is 32
        val btnUnknown = IrParsedButton(name = "Test", protocol = "CustomXYZ", nbits = 0)
        assertEquals(32, btnUnknown.effectiveBits)

        // When negative or invalid nbits is set, effectiveBits falls back to protocol default
        val btnNegative = IrParsedButton(name = "Test", protocol = "SIRC", nbits = -5)
        assertEquals(12, btnNegative.effectiveBits)
    }

    @Test
    fun testFlipperHexByteStringParsing() {
        // Space-separated little endian 4-byte Flipper format
        assertEquals(4L, IrParsedButton.parseHexOrFlipperBytes("04 00 00 00"))
        assertEquals(12L, IrParsedButton.parseHexOrFlipperBytes("0C 00 00 00"))
        assertEquals(255L, IrParsedButton.parseHexOrFlipperBytes("FF 00 00 00"))
        assertEquals(0x87EEL, IrParsedButton.parseHexOrFlipperBytes("EE 87 00 00"))
        assertEquals(0xA05DL, IrParsedButton.parseHexOrFlipperBytes("5D A0 00 00"))
        assertEquals(0x7F80L, IrParsedButton.parseHexOrFlipperBytes("80 7F 00 00"))

        // Standard 0x prefixed hex
        assertEquals(4L, IrParsedButton.parseHexOrFlipperBytes("0x04"))
        assertEquals(255L, IrParsedButton.parseHexOrFlipperBytes("0xFF"))
        assertEquals(0x1234L, IrParsedButton.parseHexOrFlipperBytes("0x1234"))

        // Standard plain hex / decimal
        assertEquals(16L, IrParsedButton.parseHexOrFlipperBytes("10")) // parsed as hex first
        assertEquals(0L, IrParsedButton.parseHexOrFlipperBytes("0"))

        // Malformed inputs safely return 0
        assertEquals(0L, IrParsedButton.parseHexOrFlipperBytes(""))
        assertEquals(0L, IrParsedButton.parseHexOrFlipperBytes("   "))
        assertEquals(0L, IrParsedButton.parseHexOrFlipperBytes("ZZ ZZ ZZ"))
    }

    @Test
    fun testRawTimingsParsingMultiLineAndSingleLine() {
        val rawIrContent = """
            Filetype: IR library file
            Version: 1
            #
            name: RAW_POWER
            type: raw
            frequency: 38000
            duty_cycle: 0.330000
            data: 9024 4488 584 544 584 1672
            data: 584 544 584 1672 584 544 584 544
        """.trimIndent()

        val remote = IrdbParser.parseIrFile(rawIrContent, "raw_sample.ir")
        assertEquals(1, remote.buttons.size)
        val btn = remote.buttons[0]
        assertEquals("RAW_POWER", btn.name)
        assertEquals("raw", btn.type)
        assertEquals(38000, btn.frequency)
        assertEquals(0.33f, btn.dutyCycle, 0.001f)
        assertEquals(14, btn.rawTimingsCount)
        assertEquals(14, btn.rawTimings.size)

        assertEquals(9024, btn.rawTimings[0])
        assertEquals(4488, btn.rawTimings[1])
        assertEquals(584, btn.rawTimings[2])
        assertEquals(544, btn.rawTimings[3])
        assertEquals(584, btn.rawTimings[btn.rawTimings.size - 2])
        assertEquals(544, btn.rawTimings[btn.rawTimings.size - 1])

        assertTrue(btn.isRaw)
        assertFalse(btn.isProtocolBased)
    }

    @Test
    fun testRawTimingsValidationBounds() {
        // Empty raw timings
        val emptyRawBtn = IrParsedButton(name = "Empty", type = "raw", rawTimings = emptyList())
        assertFalse(emptyRawBtn.isRaw)

        // Oversized raw timings (> 1024)
        val oversizedTimings = (1..1025).map { 500 }
        val oversizedBtn = IrParsedButton(name = "Oversized", type = "raw", rawTimings = oversizedTimings)
        assertTrue(oversizedBtn.rawTimings.size > 1024)

        // Zero or negative timings check
        val invalidTimings = listOf(500, 0, 500)
        assertTrue(invalidTimings.any { it <= 0 })
    }

    @Test
    fun testRoutingBetweenProtocolAndRaw() {
        val parsedBtn = IrParsedButton(name = "P1", type = "parsed", protocol = "NEC", command = "0C 00 00 00")
        assertTrue(parsedBtn.isProtocolBased)
        assertFalse(parsedBtn.isRaw)

        val rawBtn = IrParsedButton(name = "R1", type = "raw", rawTimings = listOf(9000, 4500, 560, 560))
        assertFalse(rawBtn.isProtocolBased)
        assertTrue(rawBtn.isRaw)

        val unmappedBtn = IrParsedButton(name = "U1", type = "unknown")
        assertFalse(unmappedBtn.isProtocolBased)
        assertFalse(unmappedBtn.isRaw)
    }

    @Test
    fun testIrTransmitFeedbackAndHonestStates() {
        val idleFb = IrTransmitFeedback(IrTransmitState.IDLE, "READY")
        assertEquals(IrTransmitState.IDLE, idleFb.state)

        val simFb = IrTransmitFeedback(IrTransmitState.SIMULATED, "SIMULATED: No IR signal for CH_UP")
        assertEquals(IrTransmitState.SIMULATED, simFb.state)
        assertTrue(simFb.message.startsWith("SIMULATED"))

        val failFb = IrTransmitFeedback(IrTransmitState.FAILED, "Watch not connected")
        assertEquals(IrTransmitState.FAILED, failFb.state)

        val unsuppFb = IrTransmitFeedback(IrTransmitState.UNSUPPORTED, "Unsupported signal format")
        assertEquals(IrTransmitState.UNSUPPORTED, unsuppFb.state)

        val succFb = IrTransmitFeedback(IrTransmitState.SUCCESS, "SENT: Power (SIRC 12b)")
        assertEquals(IrTransmitState.SUCCESS, succFb.state)
    }
}
