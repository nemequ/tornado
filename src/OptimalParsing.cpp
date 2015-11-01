main
{
    // 1. ЯЦЕМЕПХРЭ БНГЛНФМШЕ ЛЮРВХ
    for (i=0; i<CHUNK; i++)
    {
        matches[i] = matchp;                  // ЯНУПЮМХРЭ ЯЯШКЙС МЮ ОЕПБШИ ЛЮРВ ДКЪ ЩРНИ ОНГХЖХХ
        matchp = fill_matches(buf,i,matchp);  // ЯЦЕМЕПХРЭ БЯЕ ЛЮРВХ ЩРНИ ОНГХЖХХ Х ГЮОХЯЮРЭ ХУ Б АСТЕП
    }
    // 1.5 дНОНКМХРЭ ЯОХЯНЙ matches ЯЯШКЙЮЛХ МЮ 2/3-АЮИРНБШЕ ЯРПНЙХ
    // 2. БШАПЮРЭ МЮХКСВЬХИ ОСРЭ МЮГЮД
    iterate (CHUNK, price[i]=INT_MAX);  price[0]=0;
    for (i=0; i<CHUNK; i++)
    {
        suggest (i+1, 1, buf[i], price[i] + charPrice(buf[i]);  // ОПЕДКНФХРЭ МЮ ОНГХЖХЧ i+1 РЕЙСЫХИ ЛЮРВ + ЯХЛБНК
        lastlen = MINMATCH-1;
        for (our matches)
            while (++lastlen <= len)   // ГЮОНКМХЛ БЯЕ БЮЙЮМЯХХ НР ДКХМШ ОПЕД. ЛЮРВЮ ДН ДКХМШ МШМЕЬМЕЦН (todo: if len>256, ГЮОНКМХРЭ ОЕПБШЕ Х ОНЯКЕДМХЕ 128 ЩКЕЛЕМРНБ Х ОЕПЕЫЮЦМСРЭ БЯ╦ ОНЯЕПЕДХМЕ)
            {
                // todo: ЕЯКХ ДХЯРЮМЖХЪ ЯНБОЮДЮЕР Я НДМНИ ХГ 4 ОПЕДШДСЫХУ, РН ЖЕМЮ АСДЕР ЛЕМЭЬЕ..
                suggest (i+lastlen, lastlen, dist, price[i] + matchPrice(lastlen,dist)); // ЖЕМЮ = ЖЕМЕ РЕЙСЫЕЦН ЛЮРВЮ + ЙНДХПНБЮМХЕ МНБНЦН
            }
    }
    // 3. гЮОХЯЮРЭ НОРХЛЮКЭМШИ ОСРЭ НР ЙНМЖЮ Й МЮВЮКС
    for (i=CHUNK-1; i; i-=len[i])
    {
        push (len[i], dist[i]);
    }
    // 4. гЮЙНДХПНБЮРЭ МЮИДЕММШИ ОСРЭ Й яОЮЯЕМХЧ
    while (stack not empty)
    {
        len, dist = pop();
        encode (len,dist);
    }
}

suggest (i, len, dist, match_price)
{
    if (price[i] < match_price)        // МНБШИ БЮПХЮМР НЙЮГЮКЯЪ ДЕЬЕБКЕ
    {
        price[i] = match_price;
        len[i]  = len;
        dist[i] = dist;
    }
}
