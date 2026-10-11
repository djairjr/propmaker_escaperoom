## Notes on using the Optical Dial, results from the Dial Calibrator.
  RotaryDialUnified::setDebounce(54);   // piso: filtra ruido e pulsos da volta
  RotaryDialUnified::setMaxGap(0);       // teto: 0 = sem teto (recomendado)
// The gaps and separations can be within the same range.
RotaryDialUnified::setDigitEnd(6752);  // Timeout: The digit is not entered after silence